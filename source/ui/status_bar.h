#pragma once

#include <cstdint>
#include <limits>
#include <string>

#include "ui/panel.h"
#include "util/log.h"

namespace c8 {

/**
 * @class StatusBar
 *
 * @brief The bar at the bottom of the main window
 *
 * @details The bar shows the run state, the reason for the last stop, the
 *          program, the platform, the quirk preset, and the speeds. A click on
 *          the platform or the preset opens a menu that changes it. A click on
 *          the run state shows the line of the PC in the Code Editor.
 *
 *          A new warning or error of the log shows at the end of the bar for a
 *          few seconds. So the user sees the result of a command while the Log
 *          panel is hidden. A click on the message opens the Log panel.
 */
class StatusBar {
public:
    /// Draw the bar. Call it before the dock space, so the dock space does not cover the bar.
    auto draw(UiContext& context) -> void;

private:
    // The newest warning or error of the log, while it is new
    auto draw_problem(UiContext& context) -> void;

    uint64_t seen_version = std::numeric_limits<uint64_t>::max();
    uint64_t shown_sequence = 0;
    LogLevel level = LogLevel::warning;
    std::string message;
    double shown_at = 0.0; // ImGui::GetTime() when the message came
};

} // namespace c8
