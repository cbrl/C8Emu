#include "dock_layout.h"

#include <imgui_internal.h> // DockBuilder API

namespace c8 {

auto build_default_layout(ImGuiID dockspace_id) -> void {
    ImGui::DockBuilderRemoveNode(dockspace_id);
    ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_DockSpace);
    ImGui::DockBuilderSetNodeSize(dockspace_id, ImGui::GetMainViewport()->WorkSize);

    auto center = dockspace_id;
    auto left   = ImGui::DockBuilderSplitNode(center, ImGuiDir_Left, 0.25f, nullptr, &center);
    auto right  = ImGui::DockBuilderSplitNode(center, ImGuiDir_Right, 0.35f, nullptr, &center);

    auto left_bottom   = ImGuiID{};
    auto left_top      = ImGui::DockBuilderSplitNode(left, ImGuiDir_Up, 0.4f, nullptr, &left_bottom);
    auto registers     = ImGuiID{};
    auto stack         = ImGui::DockBuilderSplitNode(left_bottom, ImGuiDir_Right, 0.35f, nullptr, &registers);
    auto right_bottom  = ImGuiID{};
    auto right_top     = ImGui::DockBuilderSplitNode(right, ImGuiDir_Up, 0.6f, nullptr, &right_bottom);
    auto center_bottom = ImGuiID{};
    auto center_top    = ImGui::DockBuilderSplitNode(center, ImGuiDir_Up, 0.4f, nullptr, &center_bottom);

    ImGui::DockBuilderDockWindow("Chip8 Settings", left_top);
    ImGui::DockBuilderDockWindow("Registers", registers);
    ImGui::DockBuilderDockWindow("Stack", stack);
    ImGui::DockBuilderDockWindow("Watch", stack);
    ImGui::DockBuilderDockWindow("Display", center_top);
    ImGui::DockBuilderDockWindow("Memory", center_bottom);
    ImGui::DockBuilderDockWindow("Log", center_bottom);
    ImGui::DockBuilderDockWindow("Trace", center_bottom);
    ImGui::DockBuilderDockWindow("Profiler", center_bottom);
    ImGui::DockBuilderDockWindow("Code Editor", right_top);
    ImGui::DockBuilderDockWindow("Program", right_bottom);
    ImGui::DockBuilderDockWindow("Breakpoints", right_bottom);
    ImGui::DockBuilderDockWindow("Syntax Reference", right_bottom);

    ImGui::DockBuilderFinish(dockspace_id);
}


auto dock_with_on_first_use(const char* window_name) -> void {
    const auto* other = ImGui::FindWindowByName(window_name);
    if ((other != nullptr) && (other->DockId != 0)) {
        ImGui::SetNextWindowDockID(other->DockId, ImGuiCond_FirstUseEver);
    }
}

} // namespace c8
