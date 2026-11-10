#pragma once

#include <memory>
#include <vector>

#include "ui/panel.h"

namespace c8 {

/**
 * @class Ui
 *
 * @brief The main menu, the dock space, and the panels
 *
 * @details An ImGui context must exist before construction.
 */
class Ui {
public:
    Ui();

    /// Draw one frame of the UI. Call between Platform::begin_frame() and Platform::end_frame().
    auto draw(UiContext& context) -> void;

private:
    // Open and close the panels as the settings say. Then note each change in the settings.
    auto sync_open_panels(Settings& settings) -> void;

    std::vector<std::unique_ptr<Panel>> panels;
    bool reset_layout = false;
    bool panels_restored = false;
};

} // namespace c8
