#pragma once

// The memory editor header needs these before it
#include <cstdio>
#include <cstring>
#include <imgui.h>

#include <imgui_memory_editor.h>

#include <cstddef>
#include <cstdint>

#include "ui/panel.h"

namespace c8 {

class Debugger;
struct MachineState;

/**
 * @class MemoryPanel
 *
 * @brief A hex editor for the CHIP-8 memory
 *
 * @details Background colors mark the PC, the I register, the breakpoints,
 *          the watchpoints, the bytes that changed since the stop before, the
 *          program, and the font.
 */
class MemoryPanel final : public Panel {
public:
    MemoryPanel();

    [[nodiscard]]
    auto title() const -> const char* override {
        return "Memory";
    }

    auto draw(UiContext& context) -> void override;

    /// Open the panel, bring it to the front, and scroll to an address in the next frame
    auto show(size_t address) -> void;

private:
    // What the background colors show in this frame, and the edits of the frame
    struct EditorData {
        const MachineState* state         = nullptr;
        const MachineState* previous      = nullptr; // The stop before. nullptr while the program runs.
        const Debugger*     debugger      = nullptr;
        uint32_t            program_begin = 0;
        uint32_t            program_end   = 0;
        bool                edited        = false; // The user wrote a byte
    };

    // MemoryEditor::BgColorFn. user_data points to an EditorData.
    static auto background_color(const ImU8* memory, size_t address, void* user_data) -> ImU32;

    // MemoryEditor::WriteFn. user_data points to an EditorData.
    static auto write_byte(ImU8* memory, size_t address, ImU8 value, void* user_data) -> void;

    // Scroll to an address. The byte is not selected for editing.
    auto go_to(size_t address) -> void;

    MemoryEditor editor;
    EditorData data;
    bool jumped = false;
};

} // namespace c8
