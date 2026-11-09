#include "ui.h"

#include "app/source_document.h"
#include "ui/dock_layout.h"
#include "ui/main_menu.h"
#include "ui/panels/breakpoints_panel.h"
#include "ui/panels/display_panel.h"
#include "ui/panels/editor_panel.h"
#include "ui/panels/log_panel.h"
#include "ui/panels/memory_panel.h"
#include "ui/panels/program_panel.h"
#include "ui/panels/registers_panel.h"
#include "ui/panels/settings_panel.h"
#include "ui/panels/stack_panel.h"
#include "ui/panels/syntax_reference_panel.h"
#include "ui/status_bar.h"

#include <optional>

#include <imgui_internal.h>

namespace c8 {

namespace {

// The user prompt text for unsaved changes
auto prompt_reason(Command command) -> const char* {
    switch (command) {
        case Command::quit:     return "Save the changes before you exit?";
        case Command::open_rom: return "A new ROM replaces the text with its disassembly. Save the changes first?";
        default:                return "The command replaces the text. Save the changes first?";
    }
}


// Ask what to do with unsaved source changes before a command discards them
auto draw_unsaved_prompt(UiContext& context) -> void {
    static constexpr auto popup = "Unsaved Changes";

    if (context.unsaved_prompt && !ImGui::IsPopupOpen(popup)) {
        ImGui::OpenPopup(popup);
    }

    const auto* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->GetCenter(), ImGuiCond_Appearing, {0.5f, 0.5f});

    if (!ImGui::BeginPopupModal(popup, nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings)) {
        return;
    }

    if (!context.unsaved_prompt) {
        ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
        return;
    }

    const auto name = context.document.display_name();
    ImGui::Text("The code editor has unsaved changes in \"%s\".", name.c_str());
    ImGui::TextUnformatted(prompt_reason(*context.unsaved_prompt));
    ImGui::Spacing();

    const float width = ImGui::GetFontSize() * 6.0f;
    auto answer = std::optional<Command>{};

    if (ImGui::Button("Save", {width, 0.0f})) {
        answer = Command::prompt_save;
    }
    ImGui::SameLine();
    if (ImGui::Button("Discard", {width, 0.0f})) {
        answer = Command::prompt_discard;
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel", {width, 0.0f}) || ImGui::IsKeyPressed(ImGuiKey_Escape)) {
        answer = Command::prompt_cancel;
    }

    if (answer) {
        context.requests.push(*answer);
        ImGui::CloseCurrentPopup();
    }

    ImGui::EndPopup();
}

} // namespace


Ui::Ui() {
    // The order of the View menu
    panels.push_back(std::make_unique<SettingsPanel>());
    panels.push_back(std::make_unique<DisplayPanel>());
    panels.push_back(std::make_unique<RegistersPanel>());
    panels.push_back(std::make_unique<StackPanel>());
    panels.push_back(std::make_unique<ProgramPanel>());
    panels.push_back(std::make_unique<BreakpointsPanel>());
    panels.push_back(std::make_unique<EditorPanel>());
    panels.push_back(std::make_unique<SyntaxReferencePanel>());
    panels.push_back(std::make_unique<MemoryPanel>());
    panels.push_back(std::make_unique<LogPanel>());
}


auto Ui::draw(UiContext& context) -> void {
    const auto actions = draw_main_menu(context, panels);
    if (actions.reset_layout) {
        reset_layout = true;
    }

    // The status bar takes space from the main window, so it comes before the dock space
    draw_status_bar(context);

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

    draw_unsaved_prompt(context);
}

} // namespace c8
