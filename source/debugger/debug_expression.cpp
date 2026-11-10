#include "debug_expression.h"

#include "assembler/lexer.h"

#include <algorithm>
#include <array>
#include <format>
#include <limits>

namespace c8 {

enum class DebugExpression::OpCode : uint8_t {
    push,      // Push the value
    reg,       // Push the register with the id in value
    load_byte, // Replace the address on the stack with its byte
    load_word, // Replace the address on the stack with its word
    neg,
    lnot,
    bnot,
    mul, div, mod,
    add, sub,
    shl, shr,
    lt, le, gt, ge,
    eq, ne,
    band, bxor, bor,
    land, lor,
};

namespace {

using OpCode = DebugExpression::OpCode;
using Instruction = DebugExpression::Instruction;

// The deepest stack that an evaluation can use
constexpr size_t max_stack_depth = 64;

// The deepest nesting of parentheses, brackets, and unary operators
constexpr int max_nesting = 64;

// The ids of the registers after v0 to vF (0 to 15)
enum RegisterId : int64_t {
    reg_i = 16,
    reg_pc,
    reg_sp,
    reg_dt,
    reg_st,
    reg_frame,
    reg_cycles,
};


constexpr auto is_digit(char c) noexcept -> bool {
    return (c >= '0') && (c <= '9');
}

constexpr auto is_space(char c) noexcept -> bool {
    return (c == ' ') || (c == '\t');
}

auto register_id(std::string_view name) -> std::optional<int64_t> {
    const auto lowercase = to_lower_ascii(name);

    if ((lowercase.size() == 2) && (lowercase[0] == 'v') && is_hex_digit(lowercase[1])) {
        const char digit = lowercase[1];
        return is_digit(digit) ? (digit - '0') : (digit - 'a' + 10);
    }
    if (lowercase == "i")      { return reg_i; }
    if (lowercase == "pc")     { return reg_pc; }
    if (lowercase == "sp")     { return reg_sp; }
    if (lowercase == "dt")     { return reg_dt; }
    if (lowercase == "st")     { return reg_st; }
    if (lowercase == "frame")  { return reg_frame; }
    if (lowercase == "cycles") { return reg_cycles; }
    return std::nullopt;
}


struct BinaryOp {
    std::string_view text;
    OpCode           op;
    int              precedence;
};

// The longer operators come first, so "<=" does not read as "<"
constexpr auto binary_ops = std::array{
    BinaryOp{"||", OpCode::lor,  1},
    BinaryOp{"&&", OpCode::land, 2},
    BinaryOp{"==", OpCode::eq,   6},
    BinaryOp{"!=", OpCode::ne,   6},
    BinaryOp{"<=", OpCode::le,   7},
    BinaryOp{">=", OpCode::ge,   7},
    BinaryOp{"<<", OpCode::shl,  8},
    BinaryOp{">>", OpCode::shr,  8},
    BinaryOp{"|",  OpCode::bor,  3},
    BinaryOp{"^",  OpCode::bxor, 4},
    BinaryOp{"&",  OpCode::band, 5},
    BinaryOp{"<",  OpCode::lt,   7},
    BinaryOp{">",  OpCode::gt,   7},
    BinaryOp{"+",  OpCode::add,  9},
    BinaryOp{"-",  OpCode::sub,  9},
    BinaryOp{"*",  OpCode::mul,  10},
    BinaryOp{"/",  OpCode::div,  10},
    BinaryOp{"%",  OpCode::mod,  10},
};


class Parser {
public:
    Parser(std::string_view text, const SymbolTable& symbol_table) : input(text), symbols(symbol_table) {}

    auto parse() -> std::expected<std::vector<Instruction>, std::string> {
        skip_space();
        if (pos >= input.size()) {
            return std::unexpected(std::string{"The expression is empty"});
        }

        if (parse_binary(1, 0)) {
            skip_space();
            if (pos < input.size()) {
                if ((input[pos] == '=') && !input.substr(pos).starts_with("==")) {
                    fail(pos, "Use '==' to compare two values");
                }
                else {
                    fail(pos, std::format("Expected an operator, not '{}'", input[pos]));
                }
            }
        }

        if (!failure.empty()) {
            return std::unexpected(failure);
        }
        if (max_depth > max_stack_depth) {
            return std::unexpected(std::string{"The expression is too long"});
        }
        return std::move(code);
    }

private:
    // Keep the first error. Always returns false.
    auto fail(size_t column, std::string_view message) -> bool {
        if (failure.empty()) {
            failure = std::format("Column {}: {}", column + 1, message);
        }
        return false;
    }

    auto skip_space() -> void {
        while ((pos < input.size()) && is_space(input[pos])) {
            ++pos;
        }
    }

    // The label characters from `from`
    auto word_at(size_t from) const -> std::string_view {
        size_t end = from;
        while ((end < input.size()) && is_label_char(input[end])) {
            ++end;
        }
        return input.substr(from, end - from);
    }

    auto emit(OpCode op, int64_t value = 0) -> void {
        code.push_back(Instruction{.op = op, .value = value});

        // A push adds one value, a unary operator replaces one, and a binary operator replaces two with one
        switch (op) {
            case OpCode::push:
            case OpCode::reg:
                ++depth;
                break;
            case OpCode::load_byte:
            case OpCode::load_word:
            case OpCode::neg:
            case OpCode::lnot:
            case OpCode::bnot:
                break;
            default:
                --depth;
                break;
        }
        max_depth = std::max(max_depth, depth);
    }

    auto match_binary() const -> const BinaryOp* {
        const auto rest = input.substr(pos);
        const auto it = std::ranges::find_if(binary_ops, [rest](const BinaryOp& candidate) { return rest.starts_with(candidate.text); });
        return (it != binary_ops.end()) ? &*it : nullptr;
    }

    auto expect(char c) -> bool {
        skip_space();
        if ((pos >= input.size()) || (input[pos] != c)) {
            return fail(pos, std::format("Expected '{}'", c));
        }
        ++pos;
        return true;
    }

    // A value, then each binary operator of min_precedence or higher, with the value after it
    auto parse_binary(int min_precedence, int nesting) -> bool {
        if (!parse_unary(nesting)) {
            return false;
        }

        while (true) {
            skip_space();
            const auto* op = match_binary();
            if ((op == nullptr) || (op->precedence < min_precedence)) {
                return true;
            }
            pos += op->text.size();

            // A higher minimum for the right side makes the operators of the same precedence group from the left
            if (!parse_binary(op->precedence + 1, nesting)) {
                return false;
            }
            emit(op->op);
        }
    }

    auto parse_unary(int nesting) -> bool {
        skip_space();
        if (nesting > max_nesting) {
            return fail(pos, "The expression has too many levels");
        }
        if (pos >= input.size()) {
            return fail(pos, "A value must follow");
        }

        const char c = input[pos];
        if ((c != '-') && (c != '+') && (c != '~') && (c != '!')) {
            return parse_primary(nesting);
        }

        // "-10" reads as -0x10, which is easy to mistake for decimal -10, so it is an error
        if (c == '-') {
            size_t next = pos + 1;
            while ((next < input.size()) && is_space(input[next])) {
                ++next;
            }
            const auto word = word_at(next);
            const bool bare_number = !word.empty() && (is_digit(word.front()) || std::ranges::all_of(word, is_hex_digit))
                                  && !has_number_prefix(word) && !register_id(word);
            if (bare_number) {
                return std::ranges::all_of(word, is_digit)
                    ? fail(pos, std::format("Write #-{} for a decimal number, or -0x{} for a hexadecimal number", word, word))
                    : fail(pos, std::format("Write -0x{} for a negative hexadecimal number", word));
            }
        }

        ++pos;
        if (!parse_unary(nesting + 1)) {
            return false;
        }

        switch (c) {
            case '-': emit(OpCode::neg); break;
            case '~': emit(OpCode::bnot); break;
            case '!': emit(OpCode::lnot); break;
            default:  break;
        }
        return true;
    }

    auto parse_primary(int nesting) -> bool {
        const char c = input[pos];
        const size_t start = pos;

        if ((c == '(') || (c == '[')) {
            ++pos;
            if (!parse_binary(1, nesting + 1) || !expect((c == '(') ? ')' : ']')) {
                return false;
            }
            if (c == '[') {
                emit(OpCode::load_byte);
            }
            return true;
        }

        // A number with a prefix, or one that starts with a digit
        if ((c == '#') || (c == '%') || is_digit(c)) {
            ++pos;
            if ((c == '#') && (pos < input.size()) && (input[pos] == '-')) {
                ++pos;
            }
            while ((pos < input.size()) && (is_label_char(input[pos]) || ((c == '%') && (input[pos] == '#')))) {
                ++pos;
            }
            return push_number(start, input.substr(start, pos - start));
        }

        if (!is_label_char(c)) {
            return fail(pos, std::format("Expected a value, not '{}'", c));
        }

        const auto name = word_at(pos);
        pos += name.size();

        // w[address] reads a word
        if ((name == "w") || (name == "W")) {
            skip_space();
            if ((pos < input.size()) && (input[pos] == '[')) {
                ++pos;
                if (!parse_binary(1, nesting + 1) || !expect(']')) {
                    return false;
                }
                emit(OpCode::load_word);
                return true;
            }
        }

        if (const auto id = register_id(name)) {
            emit(OpCode::reg, *id);
            return true;
        }

        // A label never holds only hex digits, so such a word is a number, as in the assembler
        if (std::ranges::all_of(name, is_hex_digit)) {
            return push_number(start, name);
        }

        const auto it = symbols.find(to_lower_ascii(name));
        if (it == symbols.end()) {
            return fail(start, std::format("'{}' is not a register, a label, or a constant of the loaded program", name));
        }
        emit(OpCode::push, it->second);
        return true;
    }

    auto push_number(size_t start, std::string_view word) -> bool {
        const auto value = parse_number(word);
        if (!value) {
            return fail(start, value.error());
        }
        emit(OpCode::push, *value);
        return true;
    }

    std::string_view input;
    const SymbolTable& symbols;
    size_t pos = 0;

    std::vector<Instruction> code;
    std::string failure;
    size_t depth = 0;
    size_t max_depth = 0;
};


auto read_register(const MachineState& state, int64_t id) noexcept -> int64_t {
    if ((id >= 0) && (id < 16)) {
        return state.v[static_cast<size_t>(id)];
    }

    switch (id) {
        case reg_i:      return state.i;
        case reg_pc:     return state.pc;
        case reg_sp:     return state.sp;
        case reg_dt:     return state.delay_timer;
        case reg_st:     return state.sound_timer;
        case reg_frame:  return static_cast<int64_t>(state.frame_count);
        case reg_cycles: return static_cast<int64_t>(state.total_cycles);
        default:         return 0;
    }
}


// Arithmetic that wraps at 64 bits, so an overflow is not undefined
constexpr auto wrap(uint64_t value) noexcept -> int64_t {
    return static_cast<int64_t>(value);
}

constexpr auto apply(OpCode op, int64_t a, int64_t b) noexcept -> int64_t {
    const auto ua = static_cast<uint64_t>(a);
    const auto ub = static_cast<uint64_t>(b);

    switch (op) {
        case OpCode::mul:  return wrap(ua * ub);
        case OpCode::div:  return (b == 0) ? 0 : ((b == -1) ? wrap(0 - ua) : (a / b));
        case OpCode::mod:  return ((b == 0) || (b == -1)) ? 0 : (a % b);
        case OpCode::add:  return wrap(ua + ub);
        case OpCode::sub:  return wrap(ua - ub);
        case OpCode::shl:  return ((b >= 0) && (b <= 63)) ? wrap(ua << b) : 0;
        case OpCode::shr:  return ((b >= 0) && (b <= 63)) ? (a >> b) : 0;
        case OpCode::lt:   return (a < b) ? 1 : 0;
        case OpCode::le:   return (a <= b) ? 1 : 0;
        case OpCode::gt:   return (a > b) ? 1 : 0;
        case OpCode::ge:   return (a >= b) ? 1 : 0;
        case OpCode::eq:   return (a == b) ? 1 : 0;
        case OpCode::ne:   return (a != b) ? 1 : 0;
        case OpCode::band: return a & b;
        case OpCode::bxor: return a ^ b;
        case OpCode::bor:  return a | b;
        case OpCode::land: return ((a != 0) && (b != 0)) ? 1 : 0;
        case OpCode::lor:  return ((a != 0) || (b != 0)) ? 1 : 0;
        default:           return 0;
    }
}

} // namespace


auto DebugExpression::compile(std::string_view text, const SymbolTable& symbols) -> std::expected<DebugExpression, std::string> {
    auto parsed = Parser{text, symbols}.parse();
    if (!parsed) {
        return std::unexpected(std::move(parsed.error()));
    }

    auto expression = DebugExpression{};
    expression.code = std::move(*parsed);
    return expression;
}


auto DebugExpression::evaluate(const MachineState& state) const noexcept -> int64_t {
    auto stack = std::array<int64_t, max_stack_depth>{};
    size_t top = 0;

    // The compile checks the depth, so the guards only protect against a wrong program
    const auto pop = [&]() noexcept -> int64_t {
        return (top > 0) ? stack[--top] : 0;
    };
    const auto push = [&](int64_t value) noexcept {
        if (top < stack.size()) {
            stack[top++] = value;
        }
    };

    for (const auto& instruction : code) {
        switch (instruction.op) {
            case OpCode::push:      push(instruction.value); break;
            case OpCode::reg:       push(read_register(state, instruction.value)); break;
            case OpCode::load_byte: push(state.read(static_cast<uint32_t>(pop()))); break;
            case OpCode::load_word: push(state.read_word(static_cast<uint32_t>(pop()))); break;
            case OpCode::neg:       push(wrap(0 - static_cast<uint64_t>(pop()))); break;
            case OpCode::lnot:      push((pop() == 0) ? 1 : 0); break;
            case OpCode::bnot:      push(~pop()); break;

            default: {
                const int64_t b = pop();
                const int64_t a = pop();
                push(apply(instruction.op, a, b));
                break;
            }
        }
    }

    return pop();
}


auto MessageTemplate::compile(std::string_view text, const SymbolTable& symbols) -> std::expected<MessageTemplate, std::string> {
    auto result = MessageTemplate{};
    auto literal = std::string{};
    size_t pos = 0;

    while (pos < text.size()) {
        const char c = text[pos];

        if ((c == '{') && ((pos + 1) < text.size()) && (text[pos + 1] == '{')) {
            literal += '{';
            pos += 2;
            continue;
        }
        if ((c == '}') && ((pos + 1) < text.size()) && (text[pos + 1] == '}')) {
            literal += '}';
            pos += 2;
            continue;
        }
        if (c == '}') {
            return std::unexpected(std::format("Column {}: this '}}' has no '{{'. Write '}}}}' for a brace.", pos + 1));
        }
        if (c != '{') {
            literal += c;
            ++pos;
            continue;
        }

        const auto close = text.find('}', pos + 1);
        if (close == std::string_view::npos) {
            return std::unexpected(std::format("Column {}: this '{{' has no '}}'. Write '{{{{' for a brace.", pos + 1));
        }

        auto inner = text.substr(pos + 1, close - pos - 1);
        bool decimal = false;
        if (inner.ends_with(":d")) {
            decimal = true;
            inner.remove_suffix(2);
        }

        auto expression = DebugExpression::compile(inner, symbols);
        if (!expression) {
            return std::unexpected(std::format("In {{{}}}: {}", inner, expression.error()));
        }

        result.parts.push_back(Part{.literal = std::move(literal), .expression = std::move(*expression), .decimal = decimal});
        literal.clear();
        pos = close + 1;
    }

    if (!literal.empty()) {
        result.parts.push_back(Part{.literal = std::move(literal)});
    }
    return result;
}


auto MessageTemplate::format(const MachineState& state) const -> std::string {
    auto message = std::string{};

    for (const auto& part : parts) {
        message += part.literal;
        if (part.expression) {
            const auto value = part.expression->evaluate(state);
            message += part.decimal ? std::format("{}", value) : format_debug_value(value);
        }
    }

    return message;
}


auto format_debug_value(int64_t value) -> std::string {
    if (value < 0) {
        return std::format("-0x{:X}", 0 - static_cast<uint64_t>(value));
    }
    return std::format("0x{:X}", value);
}

} // namespace c8
