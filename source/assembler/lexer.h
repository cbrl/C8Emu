#pragma once

#include <cstdint>
#include <expected>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace c8 {

/// A word of an assembly line
struct Token {
    std::string_view text;       ///< The word as written. It points into the line.
    uint32_t         column = 0; ///< The zero-based column of the first code point
    uint32_t         length = 0; ///< The length in code points

    [[nodiscard]]
    auto operator==(const Token&) const -> bool = default;
};


/// What a word is, by its form only
enum class WordKind : uint8_t {
    mnemonic,         ///< An instruction name, for example "mov"
    directive,        ///< db, dw, org, or platform
    v_register,       ///< v0 to vF. A larger number (v10) is also this kind, and an error later.
    i_register,       ///< i
    long_keyword,     ///< long, in `mov i, long nnnn` (XO-CHIP)
    number,           ///< A number literal. Its digits can still be wrong.
    label,            ///< A name that can be a label
    label_definition, ///< A label name followed by ':'
    invalid,          ///< Any other word
};

/// Where a word is in a statement
enum class WordPosition : uint8_t {
    head,    ///< The first word. An instruction name has priority.
    operand, ///< After the first word. A bare hex number has priority, so "add" is 0xADD.
};


/**
 * @brief Split a line into words
 *
 * @details Spaces, tabs, and commas separate the words. A ';' starts a
 *          comment, which ends at the end of the line.
 */
[[nodiscard]]
auto tokenize_line(std::string_view line) -> std::vector<Token>;

/// Split a text into lines. A '\n' ends a line. A '\r' before it is removed.
[[nodiscard]]
auto split_lines(std::string_view text) -> std::vector<std::string_view>;

/// The kind of a word. The case of the letters has no effect.
[[nodiscard]]
auto classify_word(std::string_view word, WordPosition position = WordPosition::head) noexcept -> WordKind;

/**
 * @brief Parse a number literal
 *
 * @details The forms are:
 *          - Hexadecimal: `2A` or `0x2A`
 *          - Decimal: `#42` or `#-1`
 *          - Binary: `%0010_1010`
 *
 *          A '_' can separate the digits after a prefix.
 *
 * @return The value, or an error message
 */
[[nodiscard]]
auto parse_number(std::string_view word) -> std::expected<int64_t, std::string>;

/// The index of a V register word, for example 0xA for "vA". The index can be larger than 0xF.
[[nodiscard]]
auto parse_v_register(std::string_view word) noexcept -> std::optional<uint32_t>;

/// Why a name cannot be a label. std::nullopt if the name is a valid label name.
[[nodiscard]]
auto label_name_error(std::string_view name) -> std::optional<std::string>;

/// The word with ASCII letters in lowercase
[[nodiscard]]
auto to_lower_ascii(std::string_view word) -> std::string;

} // namespace c8
