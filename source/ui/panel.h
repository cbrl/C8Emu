#pragma once

#include <imgui.h>

namespace c8 {

class Chip8;
class Debugger;
class Session;
struct EmulationRate;
struct Settings;

/// Requests from the UI that the app handles after the frame
struct UiRequests {
    bool open_rom = false; ///< Show the dialog to open a ROM
    bool quit     = false; ///< End the application
};

/// What the panels can see and change
struct UiContext {
    Chip8&               chip;
    Debugger&            debugger;
    Session&             session;
    Settings&            settings;
    const EmulationRate& rate;
    ImTextureID          display_texture;
    UiRequests&          requests;
};


/**
 * @class Panel
 *
 * @brief A dockable window of the UI
 *
 * @details The title is also the ImGui window ID.
 */
class Panel {
public:
    Panel() = default;
    virtual ~Panel() = default;

    Panel(const Panel&) = delete;
    Panel(Panel&&) = delete;
    auto operator=(const Panel&) -> Panel& = delete;
    auto operator=(Panel&&) -> Panel& = delete;

    [[nodiscard]]
    virtual auto title() const -> const char* = 0;

    /// Draw the window. The UI calls this only while the panel is open.
    virtual auto draw(UiContext& context) -> void = 0;

    /// False after the user closes the window. The View menu opens it again.
    bool open = true;
};

} // namespace c8
