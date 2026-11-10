#pragma once

#include <string>

#include <imgui.h>

namespace c8 {

struct Breakpoint;

/**
 * @brief Draw the marker of a breakpoint
 *
 * @details The marker is a red dot for a plain breakpoint, an orange dot for a
 *          breakpoint with a condition or an ignore count, and a blue dot for
 *          a logpoint. A disabled breakpoint is a ring. Without a breakpoint, a
 *          place under the mouse shows a red ring.
 *
 * @param[in] breakpoint  The breakpoint, or nullptr
 * @param[in] hovered     The mouse is on the place of the marker
 */
auto draw_breakpoint_marker(ImDrawList& draw_list, ImVec2 center, float radius, const Breakpoint* breakpoint, bool hovered) -> void;

/// The settings of a breakpoint for a tooltip, one on each line. Empty for a plain breakpoint.
[[nodiscard]]
auto breakpoint_summary(const Breakpoint& breakpoint) -> std::string;

} // namespace c8
