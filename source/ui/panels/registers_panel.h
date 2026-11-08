#pragma once

#include "ui/panel.h"

namespace c8 {

/// The V registers, I, and the PC. Each value can be edited.
class RegistersPanel final : public Panel {
public:
    [[nodiscard]]
    auto title() const -> const char* override {
        return "Registers";
    }

    auto draw(UiContext& context) -> void override;
};

} // namespace c8
