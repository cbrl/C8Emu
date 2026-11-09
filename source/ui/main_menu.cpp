#include "main_menu.h"

#include "chip8/chip8.h"
#include "debugger/debugger.h"
#include "ui/command_ui.h"

#include <string>

namespace c8 {

namespace {

auto draw_file_menu(UiContext& context) -> void {
    if (!ImGui::BeginMenu("File")) {
        return;
    }

    command_menu_item(context, "Open ROM...", Command::open_rom);
    ImGui::Separator();
    command_menu_item(context, "New Source", Command::new_source);
    command_menu_item(context, "Open Source...", Command::open_source);
    command_menu_item(context, "Save Source", Command::save_source);
    command_menu_item(context, "Save Source As...", Command::save_source_as);
    command_menu_item(context, "Export ROM...", Command::export_rom);
    ImGui::Separator();
    command_menu_item(context, "Exit", Command::quit);

    ImGui::EndMenu();
}


auto draw_debug_menu(UiContext& context) -> void {
    if (!ImGui::BeginMenu("Debug")) {
        return;
    }

    command_menu_item(context, "Assemble & Load", Command::assemble_and_load);
    ImGui::Separator();
    command_menu_item(context, context.debugger.is_running() ? "Pause" : "Run", Command::run_pause);
    command_menu_item(context, "Frame Advance", Command::frame_advance);

    ImGui::Separator();
    command_menu_item(context, "Step Into", Command::step_into);
    command_menu_item(context, "Step Over", Command::step_over);
    command_menu_item(context, "Step Out", Command::step_out);

    ImGui::Separator();
    command_menu_item(context, "Reset", Command::reset);

    ImGui::Separator();
    ImGui::MenuItem("Turbo", "Hold Tab", false, false);

    ImGui::EndMenu();
}


auto draw_options_menu(UiContext& context) -> void {
    if (!ImGui::BeginMenu("Options")) {
        return;
    }

    if (ImGui::BeginMenu("Quirk Preset")) {
        const auto* current = find_preset(context.chip.quirks());

        for (const auto& preset : presets) {
            const auto name = std::string{preset.name};
            if (ImGui::MenuItem(name.c_str(), nullptr, current == &preset)) {
                context.chip.apply_preset(preset);
            }
        }

        ImGui::EndMenu();
    }

    ImGui::EndMenu();
}


auto draw_view_menu(std::span<const std::unique_ptr<Panel>> panels, MainMenuActions& actions) -> void {
    if (!ImGui::BeginMenu("View")) {
        return;
    }

    // Open or close each panel
    for (const auto& panel : panels) {
        ImGui::MenuItem(panel->title(), nullptr, &panel->open);
    }

    ImGui::Separator();

    if (ImGui::MenuItem("Reset Layout")) {
        actions.reset_layout = true;
    }

    ImGui::EndMenu();
}

} // namespace


auto draw_main_menu(UiContext& context, std::span<const std::unique_ptr<Panel>> panels) -> MainMenuActions {
    auto actions = MainMenuActions{};

    if (!ImGui::BeginMainMenuBar()) {
        return actions;
    }

    draw_file_menu(context);
    draw_debug_menu(context);
    draw_options_menu(context);
    draw_view_menu(panels, actions);

    ImGui::EndMainMenuBar();
    return actions;
}

} // namespace c8
