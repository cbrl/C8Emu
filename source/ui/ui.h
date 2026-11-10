#pragma once

#include <memory>
#include <vector>

#include "ui/panel.h"

namespace c8 {

class MemoryPanel;

/**
 * @class Ui
 *
 * @brief The main menu, the dock space, the panels, and the Help windows
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

    // The Help windows. They do not dock.
    auto draw_shortcuts_window(const UiContext& context) -> void;
    auto draw_about_window() -> void;

    std::vector<std::unique_ptr<Panel>> panels;
    MemoryPanel* memory_panel = nullptr; // In `panels`

    bool reset_layout = false;
    bool panels_restored = false;
    bool shortcuts_open = false;
    bool about_open = false;
};

} // namespace c8
