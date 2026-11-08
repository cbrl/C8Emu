#include "ui.h"

#include "ui/dock_layout.h"
#include "ui/main_menu.h"
#include "ui/panels/display_panel.h"
#include "ui/panels/editor_panel.h"
#include "ui/panels/log_panel.h"
#include "ui/panels/memory_panel.h"
#include "ui/panels/program_panel.h"
#include "ui/panels/registers_panel.h"
#include "ui/panels/settings_panel.h"
#include "ui/panels/stack_panel.h"

#include <imgui_internal.h>

namespace c8 {

Ui::Ui() {
    // The order of the View menu
    panels.push_back(std::make_unique<SettingsPanel>());
    panels.push_back(std::make_unique<DisplayPanel>());
    panels.push_back(std::make_unique<RegistersPanel>());
    panels.push_back(std::make_unique<StackPanel>());
    panels.push_back(std::make_unique<ProgramPanel>());
    panels.push_back(std::make_unique<EditorPanel>());
    panels.push_back(std::make_unique<MemoryPanel>());
    panels.push_back(std::make_unique<LogPanel>());
}


auto Ui::draw(UiContext& context) -> void {
    const auto actions = draw_main_menu(context, panels);
    if (actions.reset_layout) {
        reset_layout = true;
    }

    const ImGuiID dockspace_id = ImGui::GetID("MainDockSpace");

    // imgui.ini restores the dock layout of the last session. Without a saved layout, use the default.
    if (reset_layout || (ImGui::DockBuilderGetNode(dockspace_id) == nullptr)) {
        build_default_layout(dockspace_id);

        for (auto& panel : panels) {
            panel->open = true;
        }
        reset_layout = false;
    }

    ImGui::DockSpaceOverViewport(dockspace_id, ImGui::GetMainViewport());

    for (auto& panel : panels) {
        if (panel->open) {
            panel->draw(context);
        }
    }
}

} // namespace c8
