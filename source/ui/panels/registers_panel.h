#pragma once

#include "ui/panel.h"

namespace c8 {

/// The registers and the timers (each value can be edited), the keypad state, and the frame counters
class RegistersPanel final : public Panel {
public:
    [[nodiscard]]
    auto title() const -> const char* override {
        return "Registers";
    }

    auto draw(UiContext& context) -> void override;
};

} // namespace c8
