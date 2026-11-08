#pragma once

#include "ui/panel.h"

namespace c8 {

/// Emulation speed, run control, display, and audio settings
class SettingsPanel final : public Panel {
public:
    [[nodiscard]]
    auto title() const -> const char* override {
        return "Chip8 Settings";
    }

    auto draw(UiContext& context) -> void override;
};

} // namespace c8
