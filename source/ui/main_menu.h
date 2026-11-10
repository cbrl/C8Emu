#pragma once

#include <memory>
#include <span>

#include "ui/panel.h"

namespace c8 {

/// The actions of the main menu that the UI handles itself
struct MainMenuActions {
    bool reset_layout   = false;
    bool show_shortcuts = false; ///< Open the Keyboard Shortcuts window
    bool show_about     = false; ///< Open the About window
};

/// Draw the main menu bar
auto draw_main_menu(UiContext& context, std::span<const std::unique_ptr<Panel>> panels) -> MainMenuActions;

} // namespace c8
