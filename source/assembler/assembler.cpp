#include "assembler.h"

#include "assembler/expression.h"
#include "assembler/forms.h"
#include "assembler/lexer.h"

#include <algorithm>
#include <format>
#include <limits>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>

namespace c8 {

namespace {

// A value in an instruction or a directive
struct Field {
    std::string_view name;
    uint32_t         bits;

    [[nodiscard]]
    constexpr auto max() const noexcept -> int64_t {
        return (int64_t{1} << bits) - 1;
    }

    // A negative number is stored as two's complement, so -1 is the largest value
    [[nodiscard]]
    constexpr auto min() const noexcept -> int64_t {
        return -(int64_t{1} << (bits - 1));
    }
};

constexpr auto field_nnn  = Field{.name = "nnn", .bits = 12};
constexpr auto field_nn   = Field{.name = "nn", .bits = 8};
constexpr auto field_n    = Field{.name = "n", .bits = 4};
constexpr auto field_byte = Field{.name = "a byte", .bits = 8};
constexpr auto field_word = Field{.name = "a word", .bits = 16};
constexpr auto field_nnnn = Field{.name = "nnnn", .bits = 16};

// The largest count of `ds` and size of `align`
constexpr int64_t max_space = 0x10000;


// A value as the assembler language writes it: 0x2A, or #-5 for a negative value
auto format_value(int64_t value) -> std::string {
    return (value < 0) ? std::format("#{}", value) : std::format("0x{:X}", value);
}


// The words of an encoded instruction. Only F000 NNNN has a second word.
struct EncodedInstruction {
    uint16_t first  = 0;
    uint16_t second = 0;
};


enum class StatementKind : uint8_t {
    instruction,
    data_word, // A line with only a number
    bytes,     // db
    words,     // dw
    space,     // ds
};

// One line that writes bytes
struct Statement {
    StatementKind      kind    = StatementKind::instruction;
    uint32_t           line    = 0;
    uint32_t           address = 0;
    uint32_t           size    = 0;
    Token              head;
    std::vector<Token> operands;
    bool               failed  = false; // The first pass found an error, so the second pass skips it
};


// A constant from `name equ value`. The value is calculated when a line first needs it.
struct Constant {
    enum class State : uint8_t {
        pending,    // Not calculated yet
        evaluating, // The calculation is in progress. A use now is a loop.
        done,
        failed,     // The value has an error. The error is reported on the line of the constant.
    };

    std::string                name;       // As written
    uint32_t                   line  = 0;
    size_t                     label = 0;  // The index in AssembleResult::labels
    std::optional<Expression>  expression; // std::nullopt if the value has a syntax error
    State                      state = State::pending;
};


// A name that the source defines
struct Symbol {
    LabelKind kind  = LabelKind::label;
    size_t    index = 0; // The index in AssembleResult::labels for a label, in the constants for a constant
};


class Assembler {
public:
    Assembler(std::string_view text, const AssembleOptions& assemble_options) :
        options(assemble_options),
        source(text),
        location(assemble_options.origin) {

        result.origin = options.origin;
        result.variant = options.variant;
    }

    auto run() -> AssembleResult {
        const auto lines = split_lines(source);
        for (size_t line = 0; line < lines.size(); ++line) {
            scan_line(static_cast<uint32_t>(line), lines[line]);
        }

        // All labels have their addresses now, so each constant gets its value or reports its error
        for (size_t index = 0; index < constants.size(); ++index) {
            (void)constant_value(index);
        }

        // The image ends after the last byte that a line writes. A trailing org or align adds nothing.
        uint32_t end = options.origin;
        for (const auto& statement : statements) {
            if (!statement.failed) {
                end = std::max(end, statement.address + statement.size);
            }
        }
        result.image.assign(end - options.origin, 0);

        for (const auto& statement : statements) {
            encode(statement);
        }

        std::ranges::stable_sort(result.diagnostics, [](const Diagnostic& a, const Diagnostic& b) {
            return std::pair{a.line, a.column} < std::pair{b.line, b.column};
        });

        return std::move(result);
    }

private:
    //--------------------------------------------------------------------------------
    // Diagnostics
    //--------------------------------------------------------------------------------

    auto report(Severity severity, uint32_t line, uint32_t column, uint32_t length, std::string message) -> void {
        result.diagnostics.push_back(Diagnostic{
            .severity = severity,
            .line     = line,
            .column   = column,
            .length   = length,
            .message  = std::move(message),
        });
    }

    auto error(uint32_t line, const Token& token, std::string message) -> void {
        report(Severity::error, line, token.column, token.length, std::move(message));
    }

    auto error(uint32_t line, const ExpressionError& failure) -> void {
        report(Severity::error, line, failure.column, failure.length, failure.message);
    }

    auto warning(uint32_t line, const Token& token, std::string message) -> void {
        report(Severity::warning, line, token.column, token.length, std::move(message));
    }


    //--------------------------------------------------------------------------------
    // First pass: addresses, labels, and constants
    //--------------------------------------------------------------------------------

    auto scan_line(uint32_t line, std::string_view text) -> void {
        const auto tokens = tokenize_line(text);
        if (tokens.empty()) {
            return;
        }

        size_t index = 0;
        const Token* label = nullptr;

        if (tokens[0].text.ends_with(':')) {
            label = &tokens[0];
            index = 1;

            if ((index < tokens.size()) && tokens[index].text.ends_with(':')) {
                define_label(line, *label);
                error(line, tokens[index], "A line can define only one label");
                return;
            }
        }

        if (index == tokens.size()) {
            define_label(line, *label);
            return;
        }

        const auto& head = tokens[index];
        auto operands = std::vector<Token>(tokens.begin() + static_cast<std::ptrdiff_t>(index) + 1, tokens.end());

        // name equ value
        if (defines_constant(tokens, index)) {
            if (label != nullptr) {
                define_label(line, *label);
            }
            define_constant(line, head, operands);
            return;
        }

        const auto lowercase = to_lower_ascii(head.text);

        if (lowercase == "equ") {
            if (label != nullptr) {
                error(line, *label, "A constant has no ':'. Write: name equ value");
            }
            else {
                error(line, head, "'equ' needs a name before it. Write: name equ value");
            }
            return;
        }

        if (lowercase == "platform") {
            if (label != nullptr) {
                define_label(line, *label);
            }
            set_platform(line, head, operands);
            return;
        }

        // A label on an org or align line gets the address after the move
        if ((lowercase == "org") || (lowercase == "align")) {
            if (lowercase == "org") {
                org(line, head, operands);
            }
            else {
                align(line, head, operands);
            }
            if (label != nullptr) {
                define_label(line, *label);
            }
            return;
        }

        if (label != nullptr) {
            define_label(line, *label);
        }

        switch (classify_word(head.text, WordPosition::head)) {
            case WordKind::mnemonic: {
                const bool odd = (location % 2) != 0;

                // Warn at the first instruction of a run at odd addresses only, because the cause is before the run
                if (odd && (location != odd_run_end)) {
                    warning(line, head, std::format(
                        "The instruction is at an odd address (0x{:03X}). Data before it probably has an odd number of bytes. "
                        "'align 2' moves to an even address.",
                        location));
                }
                const auto size = instruction_size(operands);
                add_statement(StatementKind::instruction, line, head, std::move(operands), size);

                if (odd) {
                    odd_run_end = location;
                }
                break;
            }

            case WordKind::directive: {
                if (lowercase == "ds") {
                    space(line, head, std::move(operands));
                    break;
                }

                if (operands.empty()) {
                    body_started = true;
                    error(line, head, std::format("'{}' needs at least one value", head.text));
                    break;
                }

                const auto kind = (lowercase == "db") ? StatementKind::bytes : StatementKind::words;
                const auto size = static_cast<uint32_t>(operands.size()) * ((kind == StatementKind::bytes) ? 1u : 2u);
                add_statement(kind, line, head, std::move(operands), size);
                break;
            }

            case WordKind::number: {
                if (!operands.empty()) {
                    error(line, operands.front(), "A line that starts with a number holds only that number. Use 'dw' for a list of words.");
                }
                add_statement(StatementKind::data_word, line, head, {}, 2);
                break;
            }

            default: {
                error(line, head, unknown_instruction_message(head.text));

                // Keep the space of an instruction, so the addresses after this line do not move
                body_started = true;
                location += 2;
                break;
            }
        }
    }

    // The size of an instruction line. Only `mov i, long nnnn` (F000 NNNN) has 4 bytes.
    // A line with `long` keeps 4 bytes even if it has an error, so the addresses after it do not move.
    [[nodiscard]]
    static auto instruction_size(const std::vector<Token>& operands) -> uint32_t {
        const bool long_form = std::ranges::any_of(operands, [](const Token& operand) {
            return classify_word(operand.text, WordPosition::operand) == WordKind::long_keyword;
        });
        return long_form ? 4 : 2;
    }

    [[nodiscard]]
    static auto unknown_instruction_message(std::string_view word) -> std::string {
        switch (classify_word(word, WordPosition::head)) {
            case WordKind::label: {
                return std::format("'{}' is not an instruction. To define a label, put a ':' after the name.", word);
            }
            case WordKind::v_register:
            case WordKind::i_register: {
                return std::format("A line cannot start with the register '{}'", word);
            }
            default: {
                return std::format("'{}' is not an instruction", word);
            }
        }
    }

    // The address after the last byte of memory
    [[nodiscard]]
    auto memory_end() const noexcept -> uint32_t {
        return static_cast<uint32_t>(memory_size(result.variant));
    }

    auto set_platform(uint32_t line, const Token& head, const std::vector<Token>& operands) -> void {
        if (operands.size() != 1) {
            error(line, head, "'platform' needs one name: chip8, hires, schip, or xochip");
            return;
        }
        if (body_started) {
            error(line, head, "'platform' must come before the first instruction, data, or org line");
            return;
        }
        if (platform_line) {
            error(line, head, std::format("The platform is already set on line {}", *platform_line + 1));
            return;
        }

        const auto& operand = operands.front();
        const auto variant = variant_from_id(to_lower_ascii(operand.text));
        if (!variant) {
            error(line, operand, std::format("'{}' is not a platform. Use chip8, hires, schip, or xochip.", operand.text));
            return;
        }

        result.variant = *variant;
        platform_line = line;
    }

    auto add_statement(StatementKind kind, uint32_t line, const Token& head, std::vector<Token> operands, uint32_t size) -> void {
        body_started = true;

        auto statement = Statement{
            .kind     = kind,
            .line     = line,
            .address  = location,
            .size     = size,
            .head     = head,
            .operands = std::move(operands),
        };

        if ((location + size) > memory_end()) {
            statement.failed = true;

            // One error is enough. All lines after this one are also outside memory.
            if (!overflow_reported) {
                error(line, head, std::format(
                    "The program does not fit in memory. This line ends at 0x{:X}, but {} memory ends at 0x{:X}.",
                    location + size, to_string(result.variant), memory_end()));
                overflow_reported = true;
            }
        }

        location += size;
        statements.push_back(std::move(statement));
    }

    auto org(uint32_t line, const Token& head, const std::vector<Token>& operands) -> void {
        body_started = true;

        if (operands.size() != 1) {
            error(line, head, "'org' needs one address");
            return;
        }

        const auto& operand = operands.front();
        const auto value = first_pass_value(line, operand, "org");
        if (!value) {
            return;
        }

        if (*value < static_cast<int64_t>(location)) {
            error(line, operand, std::format("'org' cannot move back. The address is already 0x{:03X}.", location));
        }
        else if (*value > static_cast<int64_t>(memory_end())) {
            error(line, operand, std::format("{} is past the end of memory (0x{:X})", format_value(*value), memory_end()));
        }
        else {
            location = static_cast<uint32_t>(*value);
        }
    }

    auto align(uint32_t line, const Token& head, const std::vector<Token>& operands) -> void {
        body_started = true;

        if (operands.size() != 1) {
            error(line, head, "'align' needs one size, a power of two");
            return;
        }

        const auto& operand = operands.front();
        const auto value = first_pass_value(line, operand, "align");
        if (!value) {
            return;
        }

        if ((*value < 1) || (*value > max_space) || ((*value & (*value - 1)) != 0)) {
            error(line, operand, std::format(
                "The size of 'align' must be a power of two from 1 to 0x{:X}, not {}", max_space, format_value(*value)));
            return;
        }

        const auto size = static_cast<uint32_t>(*value);
        const auto aligned = ((location + size - 1) / size) * size;
        if (aligned > memory_end()) {
            error(line, operand, std::format("'align' moves to 0x{:X}, past the end of memory (0x{:X})", aligned, memory_end()));
            return;
        }

        location = aligned;
    }

    auto space(uint32_t line, const Token& head, std::vector<Token> operands) -> void {
        body_started = true;

        if (operands.empty() || (operands.size() > 2)) {
            error(line, head, "'ds' needs a count and can have a fill byte. Write: ds count [fill]");
            return;
        }

        const auto count = first_pass_value(line, operands.front(), "ds");
        if (!count) {
            return;
        }

        if ((*count < 0) || (*count > max_space)) {
            error(line, operands.front(), std::format("The count of 'ds' must be 0 to 0x{:X}, not {}", max_space, format_value(*count)));
            return;
        }

        add_statement(StatementKind::space, line, head, std::move(operands), static_cast<uint32_t>(*count));
    }

    // Report an error and return false if a name cannot be defined
    auto check_new_name(uint32_t line, const Token& token, std::string_view name, std::string_view kind) -> bool {
        if (const auto problem = label_name_error(name, kind)) {
            error(line, token, *problem);
            return false;
        }

        if (const auto it = symbols.find(to_lower_ascii(name)); it != symbols.end()) {
            const auto& symbol = it->second;
            const auto other_line = (symbol.kind == LabelKind::label)
                ? result.labels[symbol.index].line
                : constants[symbol.index].line;
            error(line, token, std::format("The name '{}' is already defined on line {}", name, other_line + 1));
            return false;
        }

        return true;
    }

    auto define_label(uint32_t line, const Token& token) -> void {
        auto name = token.text;
        name.remove_suffix(1); // ':'

        if (!check_new_name(line, token, name, "label")) {
            return;
        }

        auto key = to_lower_ascii(name);
        symbols.emplace(key, Symbol{.kind = LabelKind::label, .index = result.labels.size()});
        result.labels.push_back(Label{
            .name  = std::move(key),
            .kind  = LabelKind::label,
            .value = location,
            .line  = line,
        });
    }

    // name equ value. operands[0] is "equ".
    auto define_constant(uint32_t line, const Token& name, const std::vector<Token>& operands) -> void {
        if (!check_new_name(line, name, name.text, "constant")) {
            return;
        }

        auto constant = Constant{
            .name  = std::string{name.text},
            .line  = line,
            .label = result.labels.size(),
            .state = Constant::State::failed,
        };

        if (operands.size() < 2) {
            error(line, operands.front(), "'equ' needs a value. Write: name equ value");
        }
        else {
            if (operands.size() > 2) {
                error(line, operands[2], "A constant has one value. Put an expression with spaces in parentheses.");
            }

            const auto& value = operands[1];
            if (auto expression = parse_expression(value.text, value.column)) {
                constant.expression = std::move(*expression);
                constant.state = Constant::State::pending;
            }
            else {
                error(line, expression.error());
            }
        }

        auto key = to_lower_ascii(name.text);
        symbols.emplace(key, Symbol{.kind = LabelKind::constant, .index = constants.size()});
        result.labels.push_back(Label{
            .name  = std::move(key),
            .kind  = LabelKind::constant,
            .known = false,
            .line  = line,
        });
        constants.push_back(std::move(constant));
    }


    //--------------------------------------------------------------------------------
    // Values
    //--------------------------------------------------------------------------------

    // The value of a label or a constant. In the first pass, only the names before the line are defined.
    auto name_value(std::string_view name) -> std::expected<int64_t, std::string> {
        const auto it = symbols.find(to_lower_ascii(name));

        if (it == symbols.end()) {
            if (!first_pass_user.empty()) {
                return std::unexpected(std::format(
                    "'{}' is not defined before this line. '{}' can use only the labels and constants before it.", name, first_pass_user));
            }
            return std::unexpected(std::format("'{}' is not defined", name));
        }

        const auto& symbol = it->second;
        if (symbol.kind == LabelKind::label) {
            return result.labels[symbol.index].value;
        }
        return constant_value(symbol.index);
    }

    auto constant_value(size_t index) -> std::expected<int64_t, std::string> {
        auto& constant = constants[index];
        auto& label = result.labels[constant.label];

        switch (constant.state) {
            case Constant::State::done: {
                return label.value;
            }
            case Constant::State::failed: {
                return std::unexpected(std::format("The constant '{}' on line {} has an error", constant.name, constant.line + 1));
            }
            case Constant::State::evaluating: {
                return std::unexpected(std::format("'{}' depends on its own value", constant.name));
            }
            case Constant::State::pending: {
                break;
            }
        }

        // An evaluation adds no labels or constants, so the references stay valid
        constant.state = Constant::State::evaluating;
        const auto value = evaluate(*constant.expression, lookup);

        if (value) {
            constant.state = Constant::State::done;
            label.value = *value;
            label.known = true;
            return *value;
        }

        // In the first pass, a later label can still give the constant a value
        if (!first_pass_user.empty()) {
            constant.state = Constant::State::pending;
            return std::unexpected(std::format("'{}' has no value at this line: {}", constant.name, value.error().message));
        }

        constant.state = Constant::State::failed;
        error(constant.line, value.error());
        return std::unexpected(std::format("The constant '{}' on line {} has an error", constant.name, constant.line + 1));
    }

    // The value of an operand. Reports an error and returns std::nullopt if the operand has no value.
    auto operand_value(uint32_t line, const Token& token) -> std::optional<int64_t> {
        // Most operands are one number or one name. They need no expression, so a large data table stays fast.
        switch (classify_word(token.text, WordPosition::operand)) {
            case WordKind::number: {
                // A word with a number prefix can also be an expression ("#7%#4") or have an error.
                // The expression gives its value or the error.
                if (const auto value = parse_number(token.text)) {
                    return *value;
                }
                break;
            }
            case WordKind::label: {
                const auto value = name_value(token.text);
                if (!value) {
                    error(line, token, value.error());
                    return std::nullopt;
                }
                return *value;
            }
            default: {
                break;
            }
        }

        const auto expression = parse_expression(token.text, token.column);
        if (!expression) {
            error(line, expression.error());
            return std::nullopt;
        }

        const auto value = evaluate(*expression, lookup);
        if (!value) {
            error(line, value.error());
            return std::nullopt;
        }
        return *value;
    }

    // The value of an operand of org, ds, or align. It can use only the labels and constants before the line.
    auto first_pass_value(uint32_t line, const Token& token, std::string_view directive) -> std::optional<int64_t> {
        first_pass_user = directive;
        const auto value = operand_value(line, token);
        first_pass_user = {};
        return value;
    }

    // The value of an operand for a field. Reports an error and returns std::nullopt if the value does not fit.
    [[nodiscard]]
    auto value_of(uint32_t line, const Token& token, const Field& field) -> std::optional<uint32_t> {
        const auto value = operand_value(line, token);
        if (!value) {
            return std::nullopt;
        }

        if ((*value < field.min()) || (*value > field.max())) {
            const auto range = std::format("Use 0 to 0x{:X}, or #{} to #-1.", field.max(), field.min());

            if (classify_word(token.text, WordPosition::operand) == WordKind::number) {
                error(line, token, std::format("'{}' does not fit in {}. {}", token.text, field.name, range));
            }
            else {
                error(line, token, std::format("'{}' is {}, so it does not fit in {}. {}", token.text, format_value(*value), field.name, range));
            }
            return std::nullopt;
        }

        return static_cast<uint32_t>(*value & field.max());
    }


    //--------------------------------------------------------------------------------
    // Second pass: encoding
    //--------------------------------------------------------------------------------

    auto encode(const Statement& statement) -> void {
        if (statement.failed) {
            return;
        }

        const auto line = statement.line;
        const auto kind = (statement.kind == StatementKind::instruction) ? SpanKind::code : SpanKind::data;

        result.map.add(SourceSpan{
            .line    = line,
            .address = static_cast<uint16_t>(statement.address),
            .size    = static_cast<uint16_t>(statement.size),
            .kind    = kind,
        });

        switch (statement.kind) {
            case StatementKind::instruction: {
                if (const auto words = encode_instruction(statement)) {
                    write_word(statement.address, words->first);
                    if (statement.size == 4) {
                        write_word(statement.address + 2, words->second);
                    }
                }
                break;
            }

            case StatementKind::data_word: {
                if (const auto value = value_of(line, statement.head, field_word)) {
                    write_word(statement.address, static_cast<uint16_t>(*value));
                }
                break;
            }

            case StatementKind::bytes: {
                for (size_t i = 0; i < statement.operands.size(); ++i) {
                    if (const auto value = value_of(line, statement.operands[i], field_byte)) {
                        write_byte(statement.address + static_cast<uint32_t>(i), static_cast<uint8_t>(*value));
                    }
                }
                break;
            }

            case StatementKind::words: {
                for (size_t i = 0; i < statement.operands.size(); ++i) {
                    if (const auto value = value_of(line, statement.operands[i], field_word)) {
                        write_word(statement.address + static_cast<uint32_t>(i * 2), static_cast<uint16_t>(*value));
                    }
                }
                break;
            }

            case StatementKind::space: {
                // The image starts as zeros, so only a fill that is not zero writes bytes
                if (statement.operands.size() < 2) {
                    break;
                }
                if (const auto fill = value_of(line, statement.operands[1], field_byte); fill && (*fill != 0)) {
                    for (uint32_t i = 0; i < statement.size; ++i) {
                        write_byte(statement.address + i, static_cast<uint8_t>(*fill));
                    }
                }
                break;
            }
        }
    }

    [[nodiscard]]
    auto encode_instruction(const Statement& statement) -> std::optional<EncodedInstruction> {
        const auto line = statement.line;
        const auto mnemonic = to_lower_ascii(statement.head.text);
        const auto& operands = statement.operands;

        auto words = std::vector<WordKind>{};
        for (const auto& operand : operands) {
            words.push_back(classify_word(operand.text, WordPosition::operand));
        }

        const InstructionForm* match = nullptr;
        auto expected = std::string{};

        for (const auto& form : instruction_forms()) {
            if (form.mnemonic != mnemonic) {
                continue;
            }

            expected += expected.empty() ? "" : " or ";
            expected += format_syntax(form);

            const auto kinds = form.operands();
            if ((match == nullptr) && (kinds.size() == words.size())
                && std::ranges::equal(kinds, words, operand_fits)) {
                match = &form;
            }
        }

        if (match == nullptr) {
            error(line, statement.head, std::format("Wrong operands for '{}'. Use: {}", mnemonic, expected));
            return std::nullopt;
        }

        if (!match->available_on(result.variant)) {
            const auto* suggestion = ((match->variants & on_schip) != 0) ? "schip" : "xochip";
            error(line, statement.head, std::format(
                "'{}' needs {}. The platform is {}. Add 'platform {}' before the first instruction.",
                format_syntax(*match), platform_list(match->variants), to_string(result.variant), suggestion));
            return std::nullopt;
        }

        auto word = match->opcode;
        uint16_t second = 0;
        uint32_t x = 0;
        bool ok = true;

        for (size_t n = 0; n < operands.size(); ++n) {
            const auto& operand = operands[n];
            const auto kind = match->operands()[n];

            switch (kind) {
                case OperandKind::vx:
                case OperandKind::vy:
                case OperandKind::v0: {
                    const uint32_t reg = parse_v_register(operand.text).value_or(0);

                    if (reg > 0xF) {
                        error(line, operand, std::format("'{}' is not a register. The registers are v0 to vF.", operand.text));
                        ok = false;
                    }
                    else if (kind == OperandKind::vx) {
                        word |= static_cast<uint16_t>(reg << 8);
                        x = reg;
                    }
                    else if (kind == OperandKind::vy) {
                        word |= static_cast<uint16_t>(reg << 4);
                    }
                    else if (reg != 0) {
                        error(line, operand, std::format("'{}' with a register jumps to nnn + v0, so the register must be v0", mnemonic));
                        ok = false;
                    }
                    break;
                }

                case OperandKind::i:
                case OperandKind::long_kw: {
                    break;
                }

                case OperandKind::nnn:
                case OperandKind::nn:
                case OperandKind::n: {
                    const auto& field = (kind == OperandKind::nnn) ? field_nnn : ((kind == OperandKind::nn) ? field_nn : field_n);

                    if (const auto value = value_of(line, operand, field)) {
                        word |= static_cast<uint16_t>(*value);
                    }
                    else {
                        ok = false;
                    }
                    break;
                }

                case OperandKind::nx: {
                    if (const auto value = value_of(line, operand, field_n)) {
                        word |= static_cast<uint16_t>(*value << 8);
                    }
                    else {
                        ok = false;
                    }
                    break;
                }

                case OperandKind::nnnn: {
                    if (const auto value = value_of(line, operand, field_nnnn)) {
                        second = static_cast<uint16_t>(*value);
                    }
                    else {
                        ok = false;
                    }
                    break;
                }
            }
        }

        if (match->y_copies_x) {
            word |= static_cast<uint16_t>(x << 4);
        }

        return ok ? std::optional{EncodedInstruction{.first = word, .second = second}} : std::nullopt;
    }

    auto write_byte(uint32_t address, uint8_t value) -> void {
        const auto offset = static_cast<size_t>(address - options.origin);
        if (offset < result.image.size()) {
            result.image[offset] = value;
        }
    }

    auto write_word(uint32_t address, uint16_t value) -> void {
        write_byte(address, static_cast<uint8_t>(value >> 8));
        write_byte(address + 1, static_cast<uint8_t>(value));
    }


    const AssembleOptions& options;
    std::string_view source;
    AssembleResult result;

    std::vector<Statement> statements;

    // The address of the next byte. It has 32 bits, so a program that is too large does not wrap.
    uint32_t location = 0;
    bool overflow_reported = false;

    // A line that writes bytes or moves the address came before. A platform line must come before both.
    bool body_started = false;
    std::optional<uint32_t> platform_line;

    // The address after the last instruction at an odd address
    uint32_t odd_run_end = std::numeric_limits<uint32_t>::max();

    // Lowercase name -> label or constant
    std::unordered_map<std::string, Symbol> symbols;
    std::vector<Constant> constants;

    // The directive (org, ds, or align) whose operand the first pass calculates now. Empty at other times.
    std::string_view first_pass_user;

    NameLookup lookup = [this](std::string_view name) { return name_value(name); };
};

} // namespace


auto AssembleResult::error_count() const noexcept -> size_t {
    return static_cast<size_t>(std::ranges::count(diagnostics, Severity::error, &Diagnostic::severity));
}


auto AssembleResult::warning_count() const noexcept -> size_t {
    return static_cast<size_t>(std::ranges::count(diagnostics, Severity::warning, &Diagnostic::severity));
}


auto assemble(std::string_view source, const AssembleOptions& options) -> AssembleResult {
    return Assembler{source, options}.run();
}

} // namespace c8
