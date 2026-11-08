#pragma once

#include <cstdint>
#include <set>

#include <imgui-color-text-edit/TextEditor.h>

#include "ui/panel.h"

namespace c8 {

/**
 * @class EditorPanel
 *
 * @brief The code editor
 *
 * @details A new ROM file replaces the text with its disassembly. Compile
 *          assembles the text and runs it. The breakpoints show as line markers.
 */
class EditorPanel final : public Panel {
public:
    EditorPanel();

    [[nodiscard]]
    auto title() const -> const char* override {
        return "Code Editor";
    }

    auto draw(UiContext& context) -> void override;

private:
    // Replace the text with the disassembly of the loaded program
    auto show_disassembly(const UiContext& context) -> void;

    // Assemble the text and load the program
    auto compile(UiContext& context) -> void;

    // Show the breakpoints as line markers, if they changed
    auto update_breakpoint_markers(const UiContext& context) -> void;

    TextEditor editor;

    // The ROM file that the text shows (Session::rom_generation)
    uint64_t shown_rom = 0;

    // The breakpoints and the origin that the markers show
    std::set<uint16_t> marked_breakpoints;
    uint16_t marked_origin = 0;
    bool markers_valid = false;
};

} // namespace c8
