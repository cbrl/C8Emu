#pragma once

#include "app/commands.h"
#include "ui/panel.h"

namespace c8 {

/**
 * @brief A menu item for a command
 *
 * @details The item shows the shortcut and the description of the command
 *          (see CommandInfo). It is disabled when the command cannot run.
 *
 * @param[in] label  The text of the item. If it is nullptr, the item shows the name of the command.
 */
auto command_menu_item(UiContext& context, Command command, const char* label = nullptr) -> void;

/// A submenu with one item for each disassembly mode. An item sets the mode, then requests Command::disassemble.
auto disassemble_menu(UiContext& context, const char* label) -> void;

/**
 * @brief A button for a command
 *
 * @details The tooltip shows the shortcut and the description of the command.
 *          The button is disabled when the command cannot run.
 *
 * @param[in] label  The text of the button. If it is nullptr, the button shows the name of the command.
 */
auto command_button(UiContext& context, Command command, const char* label = nullptr) -> void;

} // namespace c8
