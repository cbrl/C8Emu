#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

#include "chip8/machine_state.h"
#include "ui/panel.h"
#include "ui/status_bar.h"

namespace c8 {

class EditorPanel;
class LogPanel;
class MemoryPanel;
class ProgramPanel;
class SpritePanel;

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
    // Add a panel to the end of the View menu
    template<typename T>
    auto add_panel() -> T*;

    // Open and close the panels as the settings say. Then note each change in the settings.
    auto sync_open_panels(Settings& settings) -> void;

    // Bring a panel to the front and show an address in it
    auto show(const UiContext& context, const ShowRequest& request) -> void;

    // Copy the machine at each stop, and give the copy of the stop before to the panels (UiContext::previous_stop)
    auto track_stops(UiContext& context) -> void;

    // The Help windows. They do not dock.
    auto draw_shortcuts_window(const UiContext& context) -> void;
    auto draw_about_window() -> void;

    StatusBar status_bar;
    std::vector<std::unique_ptr<Panel>> panels;

    // The panels that can show an address. They are in `panels`.
    EditorPanel*  editor_panel  = nullptr;
    LogPanel*     log_panel     = nullptr;
    MemoryPanel*  memory_panel  = nullptr;
    ProgramPanel* program_panel = nullptr;
    SpritePanel*  sprite_panel  = nullptr;

    // The machine at the last stop, and at the stop before it. A stop is a change of the
    // instruction count while the program is paused (a step, a pause, a step back).
    MachineState stop_state;
    MachineState previous_stop;
    std::optional<uint64_t> stop_cycles; // The instruction count of stop_state. None before the first stop.

    bool reset_layout = false;
    bool panels_restored = false;
    bool shortcuts_open = false;
    bool about_open = false;
};

} // namespace c8
