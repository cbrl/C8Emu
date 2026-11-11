#pragma once

#include <string>

#include "ui/panel.h"

namespace c8 {

/// One menu item for each platform. An item changes only the platform. The quirks stay.
auto platform_items(UiContext& context) -> void;

/// A submenu with the items of platform_items()
auto platform_menu(UiContext& context, const char* label) -> void;

/// One menu item for each preset. An item sets the platform, the quirks, and the speed.
auto preset_items(UiContext& context) -> void;

/// A submenu with the items of preset_items()
auto preset_menu(UiContext& context, const char* label) -> void;

/// A combo box that changes the platform
auto platform_combo(UiContext& context, const char* label) -> void;

/// A combo box that applies a preset. It shows "Custom" if no preset matches the machine.
auto preset_combo(UiContext& context, const char* label) -> void;

/// The name of the preset that matches the machine, or "Custom quirks"
[[nodiscard]]
auto preset_name(const Chip8& chip) -> std::string;

} // namespace c8
