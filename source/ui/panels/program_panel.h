#pragma once

#include <string>
#include <vector>

#include "ui/panel.h"

namespace c8 {

/// The instructions from the PC forward
class ProgramPanel final : public Panel {
public:
    [[nodiscard]]
    auto title() const -> const char* override {
        return "Program";
    }

    auto draw(UiContext& context) -> void override;

private:
    int instruction_count = 10;
    std::vector<std::string> instructions;
};

} // namespace c8
