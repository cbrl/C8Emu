#pragma once

#include <cstdint>

#include "ui/panel.h"

namespace c8 {

/// Run control, emulation, display, and audio settings
class SettingsPanel final : public Panel {
public:
    [[nodiscard]]
    auto title() const -> const char* override {
        return "Chip8 Settings";
    }

    auto draw(UiContext& context) -> void override;

private:
    // Quirks and speed
    auto draw_emulation(UiContext& context) -> void;

    // The fixed RNG seed option
    auto draw_rng_seed(UiContext& context) -> void;

    // The seed to use when "Fixed RNG seed" is on. The panel keeps it while the option is off.
    uint32_t seed = 1;
};

} // namespace c8
