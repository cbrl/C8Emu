#pragma once

#include <string>
#include <vector>

#include <imgui-color-text-edit/TextEditor.h>

namespace c8 {

/**
 * @brief The syntax highlighting of the CHIP-8 assembly language
 *
 * @details The highlighter reads each line the same way as the assembler:
 *          the first word is an instruction or a directive, and an operand can
 *          be an expression. The colors are:
 *          - keyword: an instruction name and `long`
 *          - preprocessor: a directive
 *          - knownIdentifier: a register
 *          - number: a number
 *          - declaration: a label definition (`name:`)
 *          - string: a constant, where it is defined and where it is used
 *          - identifier: a label where it is used
 *          - punctuation: an operator, a parenthesis, ',' and ':'
 *
 *          There are two objects with the same rules. Both live for the whole
 *          program. See set_constant_names().
 *
 * @param[in] alternate  Select the second object
 */
[[nodiscard]]
auto chip8_language(bool alternate = false) -> const TextEditor::Language*;

/**
 * @brief Set the names that the highlighter colors as constants
 *
 * @details The editor colors a line again only after the line changes, or
 *          after it gets a different language object. So after a change of
 *          the names, give the editor the other object of chip8_language().
 *          Do this at most one time before the editor draws again. Two
 *          changes give back the object that the editor already has, and the
 *          editor then colors nothing again.
 *
 * @param[in] names  The lowercase names of the constants
 *
 * @return True if the names changed
 */
auto set_constant_names(std::vector<std::string> names) -> bool;

} // namespace c8
