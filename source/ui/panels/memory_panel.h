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
 *          the program, and the font.
 */
class MemoryPanel final : public Panel {
public:
    MemoryPanel();

    [[nodiscard]]
    auto title() const -> const char* override {
        return "Memory";
    }

    auto draw(UiContext& context) -> void override;

private:
    // What the background colors show in this frame
    struct ColorSource {
        const MachineState* state         = nullptr;
        const Debugger*     debugger      = nullptr;
        uint32_t            program_begin = 0;
        uint32_t            program_end   = 0;
    };

    // MemoryEditor::BgColorFn. user_data points to a ColorSource.
    static auto background_color(const ImU8* memory, size_t address, void* user_data) -> ImU32;

    // Scroll to an address
    auto go_to(size_t address) -> void;

    MemoryEditor editor;
    ColorSource colors;
    bool jumped = false;
};

} // namespace c8
