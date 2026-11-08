#pragma once

#include "ui/panel.h"

namespace c8 {

/// The CHIP-8 screen
class DisplayPanel final : public Panel {
public:
    [[nodiscard]]
    auto title() const -> const char* override {
        return "Display";
    }

    auto draw(UiContext& context) -> void override;
};

} // namespace c8
