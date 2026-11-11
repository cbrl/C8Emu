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
 *          labels and constants of the new program. The settings keep the
 *          expressions for the next session.
 *
 *          While the program is paused, a value that changed since the stop
 *          before has the color of a changed value.
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

    // The contents of the window
    auto draw_watches(UiContext& context) -> void;

    // Make the watches from the expressions of the settings
    auto restore(const UiContext& context) -> void;

    // Copy the expressions to the settings if they changed
    auto save(UiContext& context) const -> void;

    std::vector<Watch> watches;
    std::string new_text;
    bool restored = false;
};

} // namespace c8
