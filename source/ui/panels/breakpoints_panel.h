#pragma once

#include <string>

#include "ui/panel.h"

namespace c8 {

/// The list of breakpoints, with a field to add one by its address
class BreakpointsPanel final : public Panel {
public:
    [[nodiscard]]
    auto title() const -> const char* override {
        return "Breakpoints";
    }

    auto draw(UiContext& context) -> void override;

private:
    // Add a breakpoint at the address in the text field
    auto add_from_text(UiContext& context) -> void;

    std::string address_text;
    std::string error;
};

} // namespace c8
