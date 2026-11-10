#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "debugger/debug_expression.h"
#include "ui/panel.h"

namespace c8 {

/**
 * @class WatchPanel
 *
 * @brief Debugger expressions and their values, for example `[i]` or `v0 + v1`
 *
 * @details The panel calculates the values in each frame, also while the
 *          program runs. After a load, the expressions compile again with the
 *          labels and constants of the new program.
 */
class WatchPanel final : public Panel {
public:
    [[nodiscard]]
    auto title() const -> const char* override {
        return "Watch";
    }

    auto draw(UiContext& context) -> void override;

private:
    struct Watch {
        Compiled<DebugExpression> expression;
        std::string               edit;              // The text field
        uint64_t                  symbols_version = 0; // The symbols of the last compile
    };

    std::vector<Watch> watches;
    std::string new_text;
};

} // namespace c8
