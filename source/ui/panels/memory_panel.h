#pragma once

// The memory editor header needs these before it
#include <cstdio>
#include <cstring>
#include <imgui.h>

#include <imgui_memory_editor.h>

#include "ui/panel.h"

namespace c8 {

/// A hex editor for the CHIP-8 memory
class MemoryPanel final : public Panel {
public:
    [[nodiscard]]
    auto title() const -> const char* override {
        return "Memory";
    }

    auto draw(UiContext& context) -> void override;

private:
    MemoryEditor editor;
};

} // namespace c8
