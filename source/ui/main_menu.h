#pragma once

#include <memory>
#include <span>

#include "ui/panel.h"

namespace c8 {

/// The actions of the main menu that the UI handles itself
struct MainMenuActions {
    bool reset_layout = false;
};

/// Draw the main menu bar
auto draw_main_menu(UiContext& context, std::span<const std::unique_ptr<Panel>> panels) -> MainMenuActions;

} // namespace c8
