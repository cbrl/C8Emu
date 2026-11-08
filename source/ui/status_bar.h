#pragma once

#include "ui/panel.h"

namespace c8 {

/**
 * @brief Draw the status bar at the bottom of the main window
 *
 * @details The bar shows the run state, the reason for the last stop, the
 *          program, the quirk preset, and the speeds. Call it before the dock
 *          space, so the dock space does not cover the bar.
 */
auto draw_status_bar(const UiContext& context) -> void;

} // namespace c8
