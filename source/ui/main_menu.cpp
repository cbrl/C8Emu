#include "main_menu.h"

#include "app/session.h"
#include "chip8/chip8.h"
#include "debugger/debugger.h"
#include "frontend/rom_files.h"
#include "frontend/settings.h"
#include "ui/command_ui.h"
#include "ui/machine_ui.h"
#include "util/log.h"

#include <filesystem>
#include <format>
#include <string>
#include <system_error>
#include <vector>

namespace c8 {

namespace {

// A submenu with the recent files. An item requests `command` with the file.
auto recent_menu(UiContext& context, const char* label, std::vector<std::string>& files, Command command) -> void {
    if (!ImGui::BeginMenu(label, !files.empty())) {
        return;
    }

    for (size_t index = 0; index < files.size(); ++index) {
        const auto path = path_from_utf8(files[index]);
        const auto name = std::format("{}  {}##{}", index + 1, path_to_utf8(path.filename()), index);
        if (ImGui::MenuItem(name.c_str())) {
            context.requests.open(command, path);
        }
        ImGui::SetItemTooltip("%s", files[index].c_str());
    }

    ImGui::Separator();
    if (ImGui::MenuItem("Clear List")) {
        files.clear();
    }

    ImGui::EndMenu();
}


// A submenu that selects the quick save slot. Each item tells if the slot of the program has a state.
auto state_slot_menu(UiContext& context) -> void {
    if (!ImGui::BeginMenu("State Slot")) {
        return;
    }

    const auto folder = state_folder(context.user_data);
    const auto& name = context.session.program_name();

    for (uint32_t slot = 1; slot <= Settings::state_slot_count; ++slot) {
        auto ec = std::error_code{};
        const bool used = !name.empty() && std::filesystem::exists(state_slot_file(folder, name, slot), ec);

        const auto label = std::format("Slot {}", slot);
        if (ImGui::MenuItem(label.c_str(), used ? "saved" : "empty", context.settings.state_slot == slot)) {
            context.settings.state_slot = slot;
        }
    }

    ImGui::EndMenu();
}


auto draw_file_menu(UiContext& context) -> void {
    if (!ImGui::BeginMenu("File")) {
        return;
    }

    command_menu_item(context, "Open ROM...", Command::open_rom);
    recent_menu(context, "Recent ROMs", context.settings.recent_roms, Command::open_rom_file);

    ImGui::Separator();
    command_menu_item(context, "New Source", Command::new_source);
    command_menu_item(context, "Open Source...", Command::open_source);
    recent_menu(context, "Recent Sources", context.settings.recent_sources, Command::open_source_file);
    disassemble_menu(context, "Disassemble Program");
    command_menu_item(context, "Save Source", Command::save_source);
    command_menu_item(context, "Save Source As...", Command::save_source_as);
    command_menu_item(context, "Export ROM...", Command::export_rom);

    ImGui::Separator();
    command_menu_item(context, "Quick Save State", Command::quick_save);
    command_menu_item(context, "Quick Load State", Command::quick_load);
    state_slot_menu(context);
    command_menu_item(context, "Save State As...", Command::save_state_as);
    command_menu_item(context, "Load State...", Command::load_state);

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
    command_menu_item(context, "Run to Cursor", Command::run_to_cursor);
    ImGui::SetItemTooltip("Run until the PC reaches the cursor line in the Code Editor");

    ImGui::Separator();
    command_menu_item(context, "Step Back", Command::step_back);
    ImGui::SetItemTooltip("Go back one instruction");
    command_menu_item(context, "Frame Back", Command::frame_back);
    ImGui::SetItemTooltip("Go back to the end of the previous frame");
    ImGui::MenuItem("Rewind", "Hold Backspace", false, false);

    ImGui::Separator();
    command_menu_item(context, "Toggle Breakpoint", Command::toggle_breakpoint);
    ImGui::SetItemTooltip("Toggle the breakpoint of the cursor line in the Code Editor");

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

    platform_menu(context, "Platform");
    preset_menu(context, "Preset");

    ImGui::Separator();
    ImGui::MenuItem("Auto-detect Platform", nullptr, &context.settings.auto_detect_platform);
    ImGui::SetItemTooltip("Open ROM selects the platform from the instructions of the program.\n"
                          "The extensions .c8h, .sc8, and .xo8 always select their platform.");

    const auto crc = context.session.rom_crc();
    const bool has_profile = crc && context.settings.rom_profiles.contains(*crc);
    if (ImGui::MenuItem("Forget ROM Settings", nullptr, false, has_profile)) {
        context.settings.rom_profiles.erase(*crc);
        log_info("The next open of \"{}\" selects the platform and the preset again", context.session.program_name());
    }
    ImGui::SetItemTooltip("C8Emu keeps the platform, the quirks, and the speed that you choose for a ROM,\n"
                          "and uses them when the ROM opens again. This item removes them for the loaded ROM.");

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
