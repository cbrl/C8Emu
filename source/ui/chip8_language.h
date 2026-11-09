#pragma once

#include <imgui-color-text-edit/TextEditor.h>

namespace c8 {

/**
 * @brief The syntax highlighting of the CHIP-8 assembly language
 *
 * @details The object lives for the whole program.
 */
[[nodiscard]]
auto chip8_language() -> const TextEditor::Language*;

} // namespace c8
