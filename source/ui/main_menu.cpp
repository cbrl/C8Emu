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

// A disabled menu item that names a key to hold, for example "Hold Tab"
auto held_key_item(const char* label, ImGuiKey key) -> void {
    const auto keys = std::format("Hold {}", ImGui::GetKeyName(key));
    ImGui::MenuItem(label, keys.c_str(), false, false);
}


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

    command_menu_item(context, Command::open_rom);
    recent_menu(context, "Recent ROMs", context.settings.recent_roms, Command::open_rom_file);

    ImGui::Separator();
    command_menu_item(context, Command::new_source);
    command_menu_item(context, Command::open_source);
    recent_menu(context, "Recent Sources", context.settings.recent_sources, Command::open_source_file);
    disassemble_menu(context, "Disassemble Program");
    command_menu_item(context, Command::save_source);
    command_menu_item(context, Command::save_source_as);
    command_menu_item(context, Command::export_rom);

    ImGui::Separator();
    command_menu_item(context, Command::quick_save);
    command_menu_item(context, Command::quick_load);
    state_slot_menu(context);
    command_menu_item(context, Command::save_state_as);
    command_menu_item(context, Command::load_state);

    ImGui::Separator();
    command_menu_item(context, Command::quit);

    ImGui::EndMenu();
}


auto draw_debug_menu(UiContext& context) -> void {
    if (!ImGui::BeginMenu("Debug")) {
        return;
    }

    command_menu_item(context, Command::assemble_and_load);
    ImGui::Separator();
    command_menu_item(context, Command::run_pause, context.debugger.is_running() ? "Pause" : "Run");
    command_menu_item(context, Command::frame_advance);

    ImGui::Separator();
    command_menu_item(context, Command::step_into);
    command_menu_item(context, Command::step_over);
    command_menu_item(context, Command::step_out);
    command_menu_item(context, Command::run_to_cursor);

    ImGui::Separator();
    command_menu_item(context, Command::step_back);
    command_menu_item(context, Command::frame_back);
    held_key_item("Rewind", rewind_key);

    ImGui::Separator();
    command_menu_item(context, Command::toggle_breakpoint);

    ImGui::Separator();
    command_menu_item(context, Command::reset);

    ImGui::Separator();
    held_key_item("Turbo", turbo_key);

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


auto draw_help_menu(MainMenuActions& actions) -> void {
    if (!ImGui::BeginMenu("Help")) {
        return;
    }

    if (ImGui::MenuItem("Keyboard Shortcuts")) {
        actions.show_shortcuts = true;
    }
    if (ImGui::MenuItem("About C8Emu")) {
        actions.show_about = true;
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
    draw_help_menu(actions);

    ImGui::EndMainMenuBar();
    return actions;
}

} // namespace c8
