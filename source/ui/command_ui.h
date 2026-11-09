#pragma once

#include "app/commands.h"
#include "ui/panel.h"

namespace c8 {

/// A menu item for a command. It shows the shortcut, and it is disabled when the command cannot run.
auto command_menu_item(UiContext& context, const char* label, Command command) -> void;

/// A submenu with one item for each disassembly mode. An item sets the mode, then requests Command::disassemble.
auto disassemble_menu(UiContext& context, const char* label) -> void;

/// A button for a command. Its tooltip shows the shortcut. It is disabled when the command cannot run.
auto command_button(UiContext& context, const char* label, Command command) -> void;

} // namespace c8
