#pragma once

#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace c8 {

/// True for a character that separates two words: a space, a tab, a ',', or another blank
[[nodiscard]]
constexpr auto is_separator(char c) noexcept -> bool {
    return (c == ' ') || (c == '\t') || (c == ',') || (c == '\r') || (c == '\v') || (c == '\f');
}

/// True for a UTF-8 byte that starts a code point (not a continuation byte)
[[nodiscard]]
constexpr auto starts_code_point(char c) noexcept -> bool {
    return (static_cast<unsigned char>(c) & 0xC0u) != 0x80u;
}

/// True for a hex digit: 0 to 9, a to f, or A to F
[[nodiscard]]
constexpr auto is_hex_digit(char c) noexcept -> bool {
    return ((c >= '0') && (c <= '9')) || ((c >= 'a') && (c <= 'f')) || ((c >= 'A') && (c <= 'F'));
}

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
    directive,        ///< db, dw, ds, org, align, equ, or platform
    v_register,       ///< v0 to vF. A larger number (v10) is also this kind, and an error later.
    i_register,       ///< i
    long_keyword,     ///< long, in `mov i, long nnnn` (XO-CHIP)
    number,           ///< A number literal. Its digits can still be wrong.
    label,            ///< A name that can be a label or a constant
    label_definition, ///< A label name followed by ':'
    invalid,          ///< Any other word, for example an expression such as "x+1"
};

/// Where a word is in a statement
enum class WordPosition : uint8_t {
    head,    ///< The first word. An instruction name has priority.
    operand, ///< After the first word. A bare hex number has priority, so "add" is 0xADD.
};

/// What a reserved word is. A reserved word cannot be a label or a constant.
enum class ReservedWord : uint8_t {
    register_name, ///< v0 to vF, or i
    keyword,       ///< long
    mnemonic,      ///< An instruction name
    directive,     ///< A directive name
};


/**
 * @brief Split a line into words
 *
 * @details Spaces, tabs, and commas separate the words. A ';' starts a
 *          comment, which ends at the end of the line. Inside parentheses,
 *          spaces, tabs, and commas do not separate words, so "(x + 1)" is one
 *          word. A '(' without a ')' continues the word to the end of the line
 *          or the comment.
 */
[[nodiscard]]
auto tokenize_line(std::string_view line) -> std::vector<Token>;

/// Split a text into lines. A '\n' ends a line. A '\r' before it is removed.
[[nodiscard]]
auto split_lines(std::string_view text) -> std::vector<std::string_view>;

/// The index of the first word of a statement: 1 after a label definition (a first word that ends with ':'), else 0
[[nodiscard]]
auto statement_start(std::span<const Token> tokens) noexcept -> size_t;

/// True if the statement that starts at `head` defines a constant: `name equ value`
[[nodiscard]]
auto defines_constant(std::span<const Token> tokens, size_t head) -> bool;

/// The kind of a word. The case of the letters has no effect.
[[nodiscard]]
auto classify_word(std::string_view word, WordPosition position = WordPosition::head) noexcept -> WordKind;

/**
 * @brief Parse a number literal
 *
 * @details The forms are:
 *          - Hexadecimal: `2A` or `0x2A`
 *          - Decimal: `#42` or `#-1`
 *          - Binary: `%0010_1010`, or `%..#.#.#.` for sprites. A '.' is 0 and a '#' is 1.
 *
 *          A '_' can separate the digits after a prefix.
 *
 * @return The value, or an error message
 */
[[nodiscard]]
auto parse_number(std::string_view word) -> std::expected<int64_t, std::string>;

/// True if a number has a prefix: 0x, #, or %
[[nodiscard]]
auto has_number_prefix(std::string_view word) noexcept -> bool;

/// The index of a V register word, for example 0xA for "vA". The index can be larger than 0xF.
[[nodiscard]]
auto parse_v_register(std::string_view word) noexcept -> std::optional<uint32_t>;

/// The kind of a reserved word. The case of the letters has no effect. std::nullopt if the word is not reserved.
[[nodiscard]]
auto reserved_word(std::string_view word) -> std::optional<ReservedWord>;

/**
 * @brief Why a name cannot be a label or a constant
 *
 * @param[in] name  The name, without a ':'
 * @param[in] kind  The word for the name in the message: "label" or "constant"
 *
 * @return The message, or std::nullopt if the name is valid
 */
[[nodiscard]]
auto label_name_error(std::string_view name, std::string_view kind = "label") -> std::optional<std::string>;

/// True if a character can be in a label name: a letter, a digit, '_', or '.'
[[nodiscard]]
auto is_label_char(char c) noexcept -> bool;

/// The word with ASCII letters in lowercase
[[nodiscard]]
auto to_lower_ascii(std::string_view word) -> std::string;

} // namespace c8
