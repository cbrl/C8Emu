#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "assembler/assembler.h"
#include "assembler/lexer.h"
#include "ui/panel.h"

namespace c8 {

/// A word of a source line, as the assembler reads it
struct SourceWord {
    Token            token;  ///< The whole word of the line
    std::string_view text;   ///< The word, or the part of an expression at the column. A label definition has no ':'.
    uint32_t         column = 0;
    uint32_t         length = 0;

    /// The kind of `text`. In an expression, a number is a number and a name has the kind of the name.
    WordKind kind = WordKind::invalid;

    bool head          = false; ///< The word is the first word of the statement, after a label definition
    bool in_expression = false; ///< `token` is an expression of more than one part

    std::string statement; ///< The first word of the statement in lowercase, for example "mov"
};


/// The word at a column of a line. std::nullopt at a space, a ',', or in a comment. The word points into `line`.
[[nodiscard]]
auto source_word_at(std::string_view line, uint32_t column) -> std::optional<SourceWord>;

/**
 * @brief Draw the help for a word of the code editor
 *
 * @details The help shows the problems of the word, then the information
 *          about the word:
 *          - An instruction: its forms, opcodes, effects, and platforms
 *          - A directive: its syntax and its effect
 *          - A register: its value while the program is paused
 *          - A label or a constant: its value and its line
 *          - A number: its value in hexadecimal, decimal, and binary
 *          - An expression: its value
 *
 * @return False if there is nothing to show
 */
auto draw_source_help(UiContext& context, uint32_t line, const SourceWord& word) -> bool;

/**
 * @brief The completions for the word before the cursor
 *
 * @details The first word of a statement gets the instructions and the
 *          directives. An operand gets the registers and keywords that the
 *          forms of the instruction accept at its position, and the labels
 *          and constants. Each completion starts with the letters of the
 *          word, and it replaces only the letters, digits, and '_' that the
 *          editor replaces, so a '.' before them stays.
 *
 * @param[in] line        The text of the cursor line
 * @param[in] term_start  The column where the editor starts to replace text
 * @param[in] cursor      The column of the cursor
 * @param[in] check       The last check of the text, for the labels, the constants, and the platform
 */
[[nodiscard]]
auto source_completions(std::string_view line, uint32_t term_start, uint32_t cursor, const AssembleResult& check)
    -> std::vector<std::string>;

/// The definition of the label or the constant at a column of a line. nullptr if there is none.
[[nodiscard]]
auto definition_at(const SourceDocument& document, std::string_view line, uint32_t column) -> const Label*;

/// The column of the name on the line of its definition
[[nodiscard]]
auto definition_column(std::string_view line, const Label& label) -> uint32_t;

} // namespace c8
