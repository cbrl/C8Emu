#pragma once

#include <imgui.h>

namespace c8 {

/// Dock the panels in the default layout
auto build_default_layout(ImGuiID dockspace_id) -> void;

/**
 * @brief Dock the next window with another window, if the next window has no saved place
 *
 * @details A panel that is newer than the saved layout (imgui.ini) then opens
 *          in a tab next to the other window, not as a free window. Call it
 *          before ImGui::Begin().
 *
 * @param[in] window_name  The title of the other window
 */
auto dock_with_on_first_use(const char* window_name) -> void;

} // namespace c8
