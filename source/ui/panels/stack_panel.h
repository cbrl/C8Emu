#pragma once

#include "ui/panel.h"

namespace c8 {

/// The PC and the return addresses on the stack, newest first, with the labels of the program. Each return address can be edited.
class StackPanel final : public Panel {
public:
    [[nodiscard]]
    auto title() const -> const char* override {
        return "Stack";
    }

    auto draw(UiContext& context) -> void override;
};

} // namespace c8
