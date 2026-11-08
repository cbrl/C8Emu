#include "memory_panel.h"

#include "chip8/chip8.h"

namespace c8 {

auto MemoryPanel::draw(UiContext& context) -> void {
    auto& memory = context.chip.state().memory;

    // The editor draws its own window, and its close button clears Open
    editor.Open = true;
    editor.DrawWindow(title(), memory.data(), memory.size());
    open = editor.Open;
}

} // namespace c8
