#include "expression.h"

#include "assembler/lexer.h"

#include <algorithm>
#include <cstdlib>
#include <format>
#include <optional>
#include <span>
#include <utility>

namespace c8 {

namespace {

// The largest size of a value in an expression. A product or a shift of two such values fits in 64 bits.
constexpr int64_t value_limit = int64_t{1} << 48;

// The deepest nesting of parentheses and unary operators
constexpr int max_depth = 64;

// The precedence of the operator with the lowest precedence
constexpr int lowest_precedence = 1;


// A blank between two parts. Inside parentheses, a ',' is not a blank but an error.
constexpr auto is_space(char c) noexcept -> bool {
    return is_separator(c) && (c != ',');
}

constexpr auto is_digit(char c) noexcept -> bool {
    return (c >= '0') && (c <= '9');
}

constexpr auto is_operator_char(char c) noexcept -> bool {
    return (c == '+') || (c == '-') || (c == '*') || (c == '/') || (c == '%')
        || (c == '&') || (c == '|') || (c == '^') || (c == '~');
}

auto code_points(std::string_view text) noexcept -> uint32_t {
    return static_cast<uint32_t>(std::ranges::count_if(text, starts_code_point));
}


// The precedence of a binary operator. 0 if the operator cannot join two values.
auto binary_precedence(std::string_view op) noexcept -> int {
    if (op == "|") {
        return 1;
    }
    if (op == "^") {
        return 2;
    }
    if (op == "&") {
        return 3;
    }
    if ((op == "<<") || (op == ">>")) {
        return 4;
    }
    if ((op == "+") || (op == "-")) {
        return 5;
    }
    if ((op == "*") || (op == "/") || (op == "%")) {
        return 6;
    }
    return 0;
}

// The code of an operator in an ExpressionStep
auto op_code(std::string_view op) noexcept -> char {
    if (op == "<<") {
        return '<';
    }
    if (op == ">>") {
        return '>';
    }
    return op.front();
}

// The operator of a code, as written
auto op_text(char code) -> std::string {
    if (code == '<') {
        return "<<";
    }
    if (code == '>') {
        return ">>";
    }
    return std::string(1, code);
}


// Why a name cannot be a value. std::nullopt if the name can be a label or a constant.
auto reserved_word_error(std::string_view name) -> std::optional<std::string> {
    const auto reserved = reserved_word(name);
    if (!reserved) {
        return std::nullopt;
    }

    switch (*reserved) {
        case ReservedWord::register_name: return std::format("Expected a number here, not the register '{}'", name);
        case ReservedWord::keyword:       return std::string{"Expected a number here, not the keyword 'long'"};
        case ReservedWord::mnemonic:      return std::format("'{}' is an instruction name, not a number or a label", name);
        case ReservedWord::directive:     return std::format("'{}' is a directive name, not a number or a label", name);
    }
    return std::nullopt;
}


// Why a '-' cannot come just before a number. A number without a prefix is hexadecimal, so "-10" is -0x10, not -10.
// std::nullopt if the number has a prefix.
auto bare_negative_error(const ExpressionPart& minus, const ExpressionPart& number) -> std::optional<ExpressionError> {
    if (has_number_prefix(number.text)) {
        return std::nullopt;
    }

    const bool decimal_digits = std::ranges::all_of(number.text, is_digit);
    auto message = decimal_digits
        ? std::format("'-{}' is not a number. Write #-{} for a decimal number, or -0x{} for a hexadecimal number.",
                      number.text, number.text, number.text)
        : std::format("'-{}' is not a number. Write -0x{} for a negative hexadecimal number.", number.text, number.text);

    return ExpressionError{
        .message = std::move(message),
        .column  = minus.column,
        .length  = (number.column + number.length) - minus.column,
    };
}


// The result of a binary operator, or a message if the result is not valid
auto apply(char op, int64_t a, int64_t b) -> std::expected<int64_t, std::string> {
    int64_t result = 0;

    switch (op) {
        case '+': result = a + b; break;
        case '-': result = a - b; break;
        case '&': result = a & b; break;
        case '|': result = a | b; break;
        case '^': result = a ^ b; break;

        case '*': {
            if ((a != 0) && (std::abs(b) > (value_limit / std::abs(a)))) {
                return std::unexpected("The result of '*' is too large");
            }
            result = a * b;
            break;
        }

        case '/':
        case '%': {
            if (b == 0) {
                return std::unexpected(std::format("'{}' divides by zero", op));
            }
            result = (op == '/') ? (a / b) : (a % b);
            break;
        }

        case '<':
        case '>': {
            if ((b < 0) || (b > 63)) {
                return std::unexpected(std::format("The shift count of '{}' must be #0 to #63", op_text(op)));
            }
            if (op == '>') {
                result = a >> b;
                break;
            }
            if ((b > 48) ? (a != 0) : (std::abs(a) > (value_limit >> b))) {
                return std::unexpected("The result of '<<' is too large");
            }
            result = a * (int64_t{1} << b);
            break;
        }

        default: {
            return std::unexpected(std::format("'{}' cannot join two values", op_text(op)));
        }
    }

    if ((result < -value_limit) || (result > value_limit)) {
        return std::unexpected(std::format("The result of '{}' is too large", op_text(op)));
    }
    return result;
}


// Reads the parts of an expression into steps in postfix order
class Parser {
public:
    explicit Parser(std::span<const ExpressionPart> expression_parts) : parts(expression_parts) {}

    auto run() -> std::expected<Expression, ExpressionError> {
        if (binary(lowest_precedence, 0) && (pos < parts.size())) {
            const auto& part = parts[pos];
            if (part.kind == PartKind::close) {
                fail(part, "This ')' has no matching '('");
            }
            else {
                fail(part, std::format("Expected an operator before '{}'", part.text));
            }
        }

        if (failure) {
            return std::unexpected(std::move(*failure));
        }
        return std::move(expression);
    }

private:
    // Keep the first error. Always returns false.
    auto fail(const ExpressionPart& part, std::string message) -> bool {
        if (!failure) {
            failure = ExpressionError{.message = std::move(message), .column = part.column, .length = part.length};
        }
        return false;
    }

    auto emit(ExpressionStep::Kind kind, const ExpressionPart& part, char op = 0) -> void {
        expression.steps.push_back(ExpressionStep{
            .kind   = kind,
            .op     = op,
            .column = part.column,
            .length = part.length,
        });
    }

    // A value, then each binary operator of min_precedence or higher, with the value after it
    auto binary(int min_precedence, int depth) -> bool {
        if (!unary(depth)) {
            return false;
        }

        while ((pos < parts.size()) && (parts[pos].kind == PartKind::op)) {
            const auto& op = parts[pos];
            const int precedence = binary_precedence(op.text);

            if (precedence == 0) {
                return fail(op, std::format("'{}' cannot join two values", op.text));
            }
            if (precedence < min_precedence) {
                break;
            }

            ++pos;

            // A higher minimum for the right side makes the operators of the same precedence group from the left
            if (!binary(precedence + 1, depth)) {
                return false;
            }
            emit(ExpressionStep::Kind::binary, op, op_code(op.text));
        }

        return true;
    }

    // A number, a name, a group in parentheses, or a unary operator and its value
    auto unary(int depth) -> bool {
        if (pos >= parts.size()) {
            const auto& last = parts.back();
            return fail(last, std::format("A value must follow '{}'", last.text));
        }

        const auto& part = parts[pos];
        if (depth > max_depth) {
            return fail(part, "The expression has too many levels of parentheses or signs");
        }

        switch (part.kind) {
            case PartKind::op: {
                if ((part.text != "-") && (part.text != "+") && (part.text != "~")) {
                    return fail(part, std::format("Expected a value before '{}'", part.text));
                }

                // "-10" reads as -0x10, which is easy to mistake for decimal -10, so it is an error
                if ((part.text == "-") && ((pos + 1) < parts.size()) && (parts[pos + 1].kind == PartKind::number)) {
                    if (auto problem = bare_negative_error(part, parts[pos + 1])) {
                        if (!failure) {
                            failure = std::move(*problem);
                        }
                        return false;
                    }
                }
                ++pos;
                if (!unary(depth + 1)) {
                    return false;
                }
                emit(ExpressionStep::Kind::unary, part, part.text.front());
                return true;
            }

            case PartKind::open: {
                ++pos;
                if (!binary(lowest_precedence, depth + 1)) {
                    return false;
                }
                if ((pos >= parts.size()) || (parts[pos].kind != PartKind::close)) {
                    return fail(part, "This '(' has no matching ')'");
                }
                ++pos;
                return true;
            }

            case PartKind::close: {
                const bool empty = (pos > 0) && (parts[pos - 1].kind == PartKind::open);
                return fail(part, empty ? "Expected a value between '(' and ')'" : "Expected a value before ')'");
            }

            case PartKind::number: {
                const auto value = parse_number(part.text);
                if (!value) {
                    return fail(part, value.error());
                }
                ++pos;
                emit(ExpressionStep::Kind::number, part);
                expression.steps.back().value = *value;
                return true;
            }

            case PartKind::name: {
                if (const auto problem = reserved_word_error(part.text)) {
                    return fail(part, *problem);
                }
                ++pos;
                emit(ExpressionStep::Kind::name, part);
                expression.steps.back().name = std::string{part.text};
                return true;
            }

            case PartKind::invalid: {
                return fail(part, std::format("'{}' cannot be part of an expression", part.text));
            }
        }

        return false;
    }

    std::span<const ExpressionPart> parts;
    size_t pos = 0;

    Expression expression;
    std::optional<ExpressionError> failure;
};

} // namespace


auto split_expression(std::string_view text, uint32_t column) -> std::vector<ExpressionPart> {
    auto parts = std::vector<ExpressionPart>{};

    // True where a value can start: at the start, and after an operator or a '('
    bool value_next = true;

    size_t pos = 0;
    while (pos < text.size()) {
        const char c = text[pos];

        if (is_space(c)) {
            ++pos;
            ++column;
            continue;
        }

        const size_t start = pos;
        auto kind = PartKind::invalid;

        if (c == '(') {
            kind = PartKind::open;
            ++pos;
            value_next = true;
        }
        else if (c == ')') {
            kind = PartKind::close;
            ++pos;
            value_next = false;
        }
        else if (value_next && ((c == '%') || (c == '#'))) {
            // A binary number can hold '.' and '#' (a sprite row). A decimal number can start with '-'.
            ++pos;
            if ((c == '#') && (pos < text.size()) && (text[pos] == '-')) {
                ++pos;
            }
            while ((pos < text.size()) && (is_label_char(text[pos]) || ((c == '%') && (text[pos] == '#')))) {
                ++pos;
            }
            kind = PartKind::number;
            value_next = false;
        }
        else if (is_label_char(c)) {
            while ((pos < text.size()) && is_label_char(text[pos])) {
                ++pos;
            }

            // A label never holds only hex digits, so such a word is a number
            const auto word = text.substr(start, pos - start);
            kind = (is_digit(c) || std::ranges::all_of(word, is_hex_digit)) ? PartKind::number : PartKind::name;
            value_next = false;
        }
        else if (((c == '<') || (c == '>')) && ((pos + 1) < text.size()) && (text[pos + 1] == c)) {
            kind = PartKind::op;
            pos += 2;
            value_next = true;
        }
        else if (is_operator_char(c)) {
            kind = PartKind::op;
            ++pos;
            value_next = true;
        }
        else {
            // One code point that cannot be in an expression
            ++pos;
            while ((pos < text.size()) && !starts_code_point(text[pos])) {
                ++pos;
            }
        }

        const auto part_text = text.substr(start, pos - start);
        const auto length = code_points(part_text);

        parts.push_back(ExpressionPart{.kind = kind, .text = part_text, .column = column, .length = length});
        column += length;
    }

    return parts;
}


auto parse_expression(std::string_view text, uint32_t column) -> std::expected<Expression, ExpressionError> {
    const auto parts = split_expression(text, column);
    if (parts.empty()) {
        return std::unexpected(ExpressionError{.message = "Expected a value", .column = column, .length = 0});
    }
    return Parser{parts}.run();
}


auto evaluate(const Expression& expression, const NameLookup& lookup) -> std::expected<int64_t, ExpressionError> {
    auto stack = std::vector<int64_t>{};
    stack.reserve(expression.steps.size());

    for (const auto& step : expression.steps) {
        const auto error = [&step](std::string message) {
            return std::unexpected(ExpressionError{.message = std::move(message), .column = step.column, .length = step.length});
        };

        switch (step.kind) {
            case ExpressionStep::Kind::number: {
                stack.push_back(step.value);
                break;
            }

            case ExpressionStep::Kind::name: {
                const auto value = lookup(step.name);
                if (!value) {
                    return error(value.error());
                }
                if ((*value < -value_limit) || (*value > value_limit)) {
                    return error(std::format("The value of '{}' is too large", step.name));
                }
                stack.push_back(*value);
                break;
            }

            case ExpressionStep::Kind::unary: {
                if (stack.empty()) {
                    return error("The expression is not complete");
                }
                auto& value = stack.back();
                if (step.op == '-') {
                    value = -value;
                }
                else if (step.op == '~') {
                    value = ~value;
                }
                break;
            }

            case ExpressionStep::Kind::binary: {
                if (stack.size() < 2) {
                    return error("The expression is not complete");
                }
                const int64_t right = stack.back();
                stack.pop_back();

                const auto result = apply(step.op, stack.back(), right);
                if (!result) {
                    return error(result.error());
                }
                stack.back() = *result;
                break;
            }
        }
    }

    if (stack.size() != 1) {
        return std::unexpected(ExpressionError{.message = "The expression is not complete"});
    }
    return stack.back();
}

} // namespace c8
