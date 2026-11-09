#include "assembler.h"

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
    bool             allow_label;

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

constexpr auto field_nnn  = Field{.name = "nnn", .bits = 12, .allow_label = true};
constexpr auto field_nn   = Field{.name = "nn", .bits = 8, .allow_label = false};
constexpr auto field_n    = Field{.name = "n", .bits = 4, .allow_label = false};
constexpr auto field_byte = Field{.name = "a byte", .bits = 8, .allow_label = false};
constexpr auto field_word = Field{.name = "a word", .bits = 16, .allow_label = true};


enum class StatementKind : uint8_t {
    instruction,
    data_word, // A line with only a number
    bytes,     // db
    words,     // dw
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


class Assembler {
public:
    Assembler(std::string_view text, const AssembleOptions& assemble_options) :
        options(assemble_options),
        source(text),
        location(assemble_options.origin) {

        result.origin = options.origin;
    }

    auto run() -> AssembleResult {
        const auto lines = split_lines(source);
        for (size_t line = 0; line < lines.size(); ++line) {
            scan_line(static_cast<uint32_t>(line), lines[line]);
        }

        // The image ends after the last byte that a line writes. A trailing org adds nothing.
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

    auto report(Severity severity, uint32_t line, const Token& token, std::string message) -> void {
        result.diagnostics.push_back(Diagnostic{
            .severity = severity,
            .line     = line,
            .column   = token.column,
            .length   = token.length,
            .message  = std::move(message),
        });
    }

    auto error(uint32_t line, const Token& token, std::string message) -> void {
        report(Severity::error, line, token, std::move(message));
    }

    auto warning(uint32_t line, const Token& token, std::string message) -> void {
        report(Severity::warning, line, token, std::move(message));
    }


    //--------------------------------------------------------------------------------
    // First pass: addresses and labels
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

        // A label on an org line gets the address after the org
        const auto lowercase = to_lower_ascii(head.text);
        if (lowercase == "org") {
            org(line, head, operands);
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
                        "The instruction is at an odd address (0x{:03X}). Data before it probably has an odd number of bytes.",
                        location));
                }
                add_statement(StatementKind::instruction, line, head, std::move(operands), 2);

                if (odd) {
                    odd_run_end = location;
                }
                break;
            }

            case WordKind::directive: {
                if (operands.empty()) {
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
                location += 2;
                break;
            }
        }
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

    auto add_statement(StatementKind kind, uint32_t line, const Token& head, std::vector<Token> operands, uint32_t size) -> void {
        auto statement = Statement{
            .kind     = kind,
            .line     = line,
            .address  = location,
            .size     = size,
            .head     = head,
            .operands = std::move(operands),
        };

        if ((location + size) > options.memory_size) {
            statement.failed = true;

            // One error is enough. All lines after this one are also outside memory.
            if (!overflow_reported) {
                error(line, head, std::format(
                    "The program does not fit in memory. This line ends at 0x{:X}, but memory ends at 0x{:X}.",
                    location + size, options.memory_size));
                overflow_reported = true;
            }
        }

        location += size;
        statements.push_back(std::move(statement));
    }

    auto org(uint32_t line, const Token& head, const std::vector<Token>& operands) -> void {
        if (operands.size() != 1) {
            error(line, head, "'org' needs one address");
            return;
        }

        const auto& operand = operands.front();
        if (classify_word(operand.text, WordPosition::operand) != WordKind::number) {
            error(line, operand, "'org' needs a number. A label is not allowed here.");
            return;
        }

        const auto value = parse_number(operand.text);
        if (!value) {
            error(line, operand, value.error());
            return;
        }

        if (*value < static_cast<int64_t>(location)) {
            error(line, operand, std::format("'org' cannot move back. The address is already 0x{:03X}.", location));
        }
        else if (*value > static_cast<int64_t>(options.memory_size)) {
            error(line, operand, std::format("0x{:X} is past the end of memory (0x{:X})", *value, options.memory_size));
        }
        else {
            location = static_cast<uint32_t>(*value);
        }
    }

    auto define_label(uint32_t line, const Token& token) -> void {
        auto name = token.text;
        name.remove_suffix(1); // ':'

        if (const auto problem = label_name_error(name)) {
            error(line, token, *problem);
            return;
        }

        auto key = to_lower_ascii(name);

        if (const auto it = label_index.find(key); it != label_index.end()) {
            error(line, token, std::format(
                "The label '{}' is already defined on line {}", name, result.labels[it->second].line + 1));
            return;
        }

        label_index.emplace(key, result.labels.size());
        label_locations.push_back(location);
        result.labels.push_back(Label{
            .name    = std::move(key),
            .address = static_cast<uint16_t>(location),
            .line    = line,
        });
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
                if (const auto word = encode_instruction(statement)) {
                    write_word(statement.address, *word);
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
        }
    }

    [[nodiscard]]
    auto encode_instruction(const Statement& statement) -> std::optional<uint16_t> {
        const auto line = statement.line;
        const auto mnemonic = to_lower_ascii(statement.head.text);
        const auto& operands = statement.operands;

        // The operand kinds that each word can fill
        const auto fits = [](OperandKind kind, WordKind word) {
            switch (kind) {
                case OperandKind::vx:
                case OperandKind::vy:
                case OperandKind::v0:  return word == WordKind::v_register;
                case OperandKind::i:   return word == WordKind::i_register;
                case OperandKind::nnn:
                case OperandKind::nn:
                case OperandKind::n:   return (word != WordKind::v_register) && (word != WordKind::i_register);
            }
            return false;
        };

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
                && std::ranges::equal(kinds, words, fits)) {
                match = &form;
            }
        }

        if (match == nullptr) {
            error(line, statement.head, std::format("Wrong operands for '{}'. Use: {}", mnemonic, expected));
            return std::nullopt;
        }

        auto word = match->opcode;
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

                case OperandKind::i: {
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
            }
        }

        if (match->y_copies_x) {
            word |= static_cast<uint16_t>(x << 4);
        }

        return ok ? std::optional{word} : std::nullopt;
    }

    [[nodiscard]]
    auto value_of(uint32_t line, const Token& token, const Field& field) -> std::optional<uint32_t> {
        switch (classify_word(token.text, WordPosition::operand)) {
            case WordKind::number: {
                const auto value = parse_number(token.text);
                if (!value) {
                    error(line, token, value.error());
                    return std::nullopt;
                }

                if ((*value < field.min()) || (*value > field.max())) {
                    error(line, token, std::format(
                        "'{}' does not fit in {}. Use 0 to 0x{:X}, or #{} to #-1.", token.text, field.name, field.max(), field.min()));
                    return std::nullopt;
                }

                return static_cast<uint32_t>(*value & field.max());
            }

            case WordKind::label: {
                if (!field.allow_label) {
                    error(line, token, std::format("A label is a 12-bit address, so it does not fit in {}", field.name));
                    return std::nullopt;
                }

                const auto it = label_index.find(to_lower_ascii(token.text));
                if (it == label_index.end()) {
                    error(line, token, std::format("The label '{}' is not defined", token.text));
                    return std::nullopt;
                }

                const auto address = label_locations[it->second];
                if (static_cast<int64_t>(address) > field.max()) {
                    error(line, token, std::format("The label '{}' (0x{:X}) does not fit in {}", token.text, address, field.name));
                    return std::nullopt;
                }
                return address;
            }

            case WordKind::v_register:
            case WordKind::i_register: {
                error(line, token, std::format("Expected a number here, not the register '{}'", token.text));
                return std::nullopt;
            }

            case WordKind::mnemonic: {
                error(line, token, std::format("'{}' is an instruction name, not a number or a label", token.text));
                return std::nullopt;
            }

            case WordKind::directive: {
                error(line, token, std::format("'{}' is a directive name, not a number or a label", token.text));
                return std::nullopt;
            }

            default: {
                if (token.text.starts_with('-')) {
                    error(line, token, std::format("'{}' is not a number or a label. Write a negative number as #{}.", token.text, token.text));
                }
                else {
                    error(line, token, std::format("'{}' is not a number or a label", token.text));
                }
                return std::nullopt;
            }
        }
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

    // The address after the last instruction at an odd address
    uint32_t odd_run_end = std::numeric_limits<uint32_t>::max();

    // Lowercase name -> index in result.labels and label_locations
    std::unordered_map<std::string, size_t> label_index;
    std::vector<uint32_t> label_locations;
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
