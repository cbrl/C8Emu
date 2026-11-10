#pragma once

#include <cstdint>
#include <expected>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "chip8/machine_state.h"

namespace c8 {

/// The names that a debugger expression can use: the labels and constants of the loaded program. The keys are lowercase.
using SymbolTable = std::map<std::string, int64_t, std::less<>>;


/**
 * @class DebugExpression
 *
 * @brief A compiled expression of the debugger: a condition, a value in a log message, or a watch
 *
 * @details The syntax is the syntax of C, with the numbers of the assembler:
 *          - Numbers: `2A` and `0x2A` are hexadecimal, `#42` is decimal, and
 *            `%0010_1010` is binary. A '-' just before a number without a
 *            prefix is an error, because "-10" would be -0x10.
 *          - Registers: `v0` to `vF`, `i`, `pc`, `sp`, `dt` (the delay
 *            timer), and `st` (the sound timer). Counters: `frame` and
 *            `cycles`, since the reset.
 *          - Memory: `[address]` reads a byte, and `w[address]` reads a
 *            big-endian word.
 *          - Operators, from low to high precedence: `||`, `&&`, `|`, `^`,
 *            `&`, `== !=`, `< <= > >=`, `<< >>`, `+ -`, `* / %`, and the
 *            unary `- + ~ !`. A comparison gives 1 if it is true and 0 if not.
 *          - Other names: the labels and the constants of the loaded program.
 *            The expression keeps their values from the compile.
 *
 *          An evaluation never fails. A division by zero gives 0, a shift by
 *          more than 63 bits gives 0, and the arithmetic wraps at 64 bits.
 */
class DebugExpression {
public:
    DebugExpression() = default;

    /// Compile a text. Returns a message for the user if the text has an error.
    [[nodiscard]]
    static auto compile(std::string_view text, const SymbolTable& symbols) -> std::expected<DebugExpression, std::string>;

    /// The value of the expression in a machine state
    [[nodiscard]]
    auto evaluate(const MachineState& state) const noexcept -> int64_t;

    enum class OpCode : uint8_t;

    /// One step of the evaluation, in postfix order
    struct Instruction {
        OpCode  op;
        int64_t value = 0;
    };

private:
    std::vector<Instruction> code;
};


/**
 * @class MessageTemplate
 *
 * @brief The message of a logpoint, with values of expressions in it
 *
 * @details `{expression}` gives the value in hexadecimal, for example 0x2A.
 *          `{expression:d}` gives the value in decimal. `{{` and `}}` give a
 *          brace.
 */
class MessageTemplate {
public:
    MessageTemplate() = default;

    /// Compile a text. Returns a message for the user if the text has an error.
    [[nodiscard]]
    static auto compile(std::string_view text, const SymbolTable& symbols) -> std::expected<MessageTemplate, std::string>;

    /// The message in a machine state
    [[nodiscard]]
    auto format(const MachineState& state) const -> std::string;

private:
    struct Part {
        std::string                    literal;    // The text before the value
        std::optional<DebugExpression> expression;
        bool                           decimal = false;
    };

    std::vector<Part> parts;
};


/// A value in hexadecimal, for example 0x2A or -0x5
[[nodiscard]]
auto format_debug_value(int64_t value) -> std::string;


/**
 * @struct Compiled
 *
 * @brief The text of a debugger expression or message, and its compiled form
 *
 * @details The text stays when it does not compile, so it can compile again
 *          with the symbols of the next program.
 */
template<typename T>
struct Compiled {
    std::string      text;  ///< As the user wrote it, without spaces at the ends. Empty if not set.
    std::optional<T> value; ///< The compiled form. std::nullopt if the text is empty or has an error.
    std::string      error; ///< The error of the last compile. Empty if the text compiled.

    /// True if the text is not empty
    [[nodiscard]]
    auto is_set() const noexcept -> bool {
        return !text.empty();
    }

    /// Compile the text again, for example with the symbols of a new program
    auto compile(const SymbolTable& symbols) -> void {
        value.reset();
        error.clear();
        if (text.empty()) {
            return;
        }

        auto result = T::compile(text, symbols);
        if (result) {
            value = std::move(*result);
        }
        else {
            error = std::move(result.error());
        }
    }

    /// Set a new text and compile it. Returns the error, if any.
    auto assign(std::string_view new_text, const SymbolTable& symbols) -> std::expected<void, std::string> {
        const auto first = new_text.find_first_not_of(" \t");
        const auto last = new_text.find_last_not_of(" \t");
        text = (first == std::string_view::npos) ? std::string{} : std::string{new_text.substr(first, last - first + 1)};

        compile(symbols);
        if (!error.empty()) {
            return std::unexpected(error);
        }
        return {};
    }
};

} // namespace c8
