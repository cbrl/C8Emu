#include "ui.h"

#include "app/source_document.h"
#include "chip8/chip8.h"
#include "debugger/debugger.h"
#include "frontend/settings.h"
#include "ui/dock_layout.h"
#include "ui/main_menu.h"
#include "ui/panels/breakpoints_panel.h"
#include "ui/panels/display_panel.h"
#include "ui/panels/editor_panel.h"
#include "ui/panels/keypad_panel.h"
#include "ui/panels/log_panel.h"
#include "ui/panels/memory_panel.h"
#include "ui/panels/profiler_panel.h"
#include "ui/panels/program_panel.h"
#include "ui/panels/registers_panel.h"
#include "ui/panels/settings_panel.h"
#include "ui/panels/sprite_panel.h"
#include "ui/panels/stack_panel.h"
#include "ui/panels/syntax_reference_panel.h"
#include "ui/panels/trace_panel.h"
#include "ui/panels/watch_panel.h"

#include <algorithm>
#include <optional>
#include <string>
#include <string_view>

#include <imgui_internal.h>

#include <SDL3/SDL_version.h>

namespace c8 {

namespace {

// The user prompt text for unsaved changes
auto prompt_reason(Command command) -> const char* {
    switch (command) {
        case Command::quit:        return "Save the changes before you exit?";
        case Command::open_rom:
        case Command::open_rom_file: return "A new ROM replaces the text with its disassembly. Save the changes first?";
        case Command::disassemble: return "The disassembly replaces the text. Save the changes first?";
        default:                   return "The command replaces the text. Save the changes first?";
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

    // Enter saves, and Escape cancels
    if (ImGui::Button("Save", {width, 0.0f}) || ImGui::IsKeyPressed(ImGuiKey_Enter) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter)) {
        answer = Command::prompt_save;
    }
    ImGui::SetItemDefaultFocus();
    ImGui::SetItemTooltip("Enter");
    ImGui::SameLine();
    if (ImGui::Button("Discard", {width, 0.0f})) {
        answer = Command::prompt_discard;
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel", {width, 0.0f}) || ImGui::IsKeyPressed(ImGuiKey_Escape)) {
        answer = Command::prompt_cancel;
    }
    ImGui::SetItemTooltip("Escape");

    if (answer) {
        context.requests.push(*answer);
        ImGui::CloseCurrentPopup();
    }

    ImGui::EndPopup();
}


// A table of names and keys, for the Keyboard Shortcuts window. The key column has the same width in each table.
auto begin_keys_table(const char* id) -> bool {
    if (!ImGui::BeginTable(id, 2, ImGuiTableFlags_RowBg)) {
        return false;
    }
    ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableSetupColumn("Keys", ImGuiTableColumnFlags_WidthFixed, ImGui::GetFontSize() * 9.0f);
    return true;
}


auto keys_row(std::string_view name, std::string_view keys) -> void {
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::TextUnformatted(name.data(), name.data() + name.size());
    ImGui::TableNextColumn();
    ImGui::TextUnformatted(keys.data(), keys.data() + keys.size());
}

} // namespace


template<typename T>
auto Ui::add_panel() -> T* {
    auto panel = std::make_unique<T>();
    auto* added = panel.get();
    panels.push_back(std::move(panel));
    return added;
}


Ui::Ui() {
    // The order of the View menu
    add_panel<SettingsPanel>();
    add_panel<DisplayPanel>();
    add_panel<KeypadPanel>();
    add_panel<RegistersPanel>();
    add_panel<StackPanel>();
    add_panel<WatchPanel>();
    program_panel = add_panel<ProgramPanel>();
    add_panel<BreakpointsPanel>();
    add_panel<TracePanel>();
    add_panel<ProfilerPanel>();
    editor_panel = add_panel<EditorPanel>();
    add_panel<SyntaxReferencePanel>();
    memory_panel = add_panel<MemoryPanel>();
    sprite_panel = add_panel<SpritePanel>();
    log_panel = add_panel<LogPanel>();
}


auto Ui::draw(UiContext& context) -> void {
    sync_open_panels(context.settings);
    track_stops(context);

    const auto actions = draw_main_menu(context, panels);
    if (actions.reset_layout) {
        reset_layout = true;
    }
    shortcuts_open |= actions.show_shortcuts;
    about_open |= actions.show_about;

    // The status bar takes space from the main window, so it comes before the dock space
    status_bar.draw(context);

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

    if (context.requests.show) {
        show(context, *context.requests.show);
    }

    // An edit is not a stop. The next stop compares with the edited values.
    if (context.requests.state_edited && stop_cycles) {
        stop_state = context.chip.state();
    }

    draw_shortcuts_window(context);
    draw_about_window();
    draw_unsaved_prompt(context);
}


auto Ui::sync_open_panels(Settings& settings) -> void {
    auto& closed = settings.closed_panels;

    // The first frame opens the panels of the last session
    if (!panels_restored) {
        panels_restored = true;
        for (auto& panel : panels) {
            panel->open = std::ranges::find(closed, panel->title()) == closed.end();
        }
        return;
    }

    auto now_closed = std::vector<std::string>{};
    for (const auto& panel : panels) {
        if (!panel->open) {
            now_closed.emplace_back(panel->title());
        }
    }
    if (now_closed != closed) {
        closed = std::move(now_closed);
    }
}



auto Ui::track_stops(UiContext& context) -> void {
    if (context.debugger.is_running()) {
        context.previous_stop = nullptr;
        return;
    }

    const auto& state = context.chip.state();
    if (state.total_cycles != stop_cycles) {
        // A load or a reset sets the count to 0. The values of the old program are not changes.
        const bool restart = !stop_cycles || (state.total_cycles == 0) || (state.memory.size() != stop_state.memory.size());

        previous_stop = restart ? state : std::move(stop_state);
        stop_state = state;
        stop_cycles = state.total_cycles;
    }

    context.previous_stop = &previous_stop;
}


auto Ui::show(const UiContext& context, const ShowRequest& request) -> void {
    switch (request.target) {
        case ShowTarget::code_editor: {
            // Data, an address outside the program, or a program from another text has no line
            if (const auto line = context.document.lines().line_of(static_cast<uint16_t>(request.address))) {
                editor_panel->show_line(*line);
            }
            else {
                program_panel->show(request.address);
            }
            break;
        }
        case ShowTarget::program: {
            program_panel->show(request.address);
            break;
        }
        case ShowTarget::memory: {
            memory_panel->show(request.address);
            break;
        }
        case ShowTarget::sprites: {
            sprite_panel->show(request.address);
            break;
        }
        case ShowTarget::log: {
            log_panel->bring_to_front();
            break;
        }
    }
}


auto Ui::draw_shortcuts_window(const UiContext& context) -> void {
    if (!shortcuts_open) {
        return;
    }

    ImGui::SetNextWindowSize({ImGui::GetFontSize() * 26.0f, ImGui::GetFontSize() * 36.0f}, ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Keyboard Shortcuts", &shortcuts_open, ImGuiWindowFlags_NoDocking)) {
        ImGui::End();
        return;
    }

    // The commands that have a shortcut, in the groups of the command table
    auto category = std::string_view{};
    bool table_open = false;
    for (const auto& info : all_commands()) {
        if (info.chord == ImGuiKey_None) {
            continue;
        }
        if (info.category != category) {
            if (table_open) {
                ImGui::EndTable();
            }
            category = info.category;
            const auto heading = std::string{category};
            ImGui::SeparatorText(heading.c_str());
            table_open = begin_keys_table(("##" + heading).c_str());
        }
        if (table_open) {
            keys_row(info.name, shortcut_text(info.command));
        }
    }
    if (table_open) {
        ImGui::EndTable();
    }

    ImGui::SeparatorText("Held Keys");
    if (begin_keys_table("##held")) {
        keys_row("Turbo", ImGui::GetKeyName(turbo_key));
        keys_row("Rewind", ImGui::GetKeyName(rewind_key));
        ImGui::EndTable();
    }

    ImGui::SeparatorText("Code Editor");
    if (begin_keys_table("##editor")) {
        keys_row("Autocomplete", "Ctrl+Space");
        keys_row("Go to Definition", "F12");
        keys_row("Go to Line", "Ctrl+Shift+G");
        keys_row("Find and Replace", "Ctrl+F");
        keys_row("Find Next", "Ctrl+G");
        keys_row("Toggle Comment", "Ctrl+/");
        keys_row("Undo", "Ctrl+Z");
        keys_row("Redo", "Ctrl+Y");
        ImGui::EndTable();
    }

    // The keymap of the platform gives the keyboard keys
    ImGui::SeparatorText("CHIP-8 Keypad");
    ImGui::TextDisabled("Each CHIP-8 key, and the keyboard key that presses it");
    draw_keypad_map(context);

    ImGui::End();
}


auto Ui::draw_about_window() -> void {
    if (!about_open) {
        return;
    }

    const auto* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->GetCenter(), ImGuiCond_Appearing, {0.5f, 0.5f});
    ImGui::SetNextWindowSize({ImGui::GetFontSize() * 28.0f, 0.0f}, ImGuiCond_Always);

    constexpr auto flags = ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse;
    if (ImGui::Begin("About C8Emu", &about_open, flags)) {
        ImGui::TextUnformatted("C8Emu");
        ImGui::TextDisabled("A CHIP-8 emulator with an assembler and a debugger");
        ImGui::Separator();

        ImGui::Text("Platforms");
		ImGui::BulletText("CHIP-8 (COSMAC VIP)");
		ImGui::BulletText("Hires CHIP-8");
		ImGui::BulletText("SUPER-CHIP");
		ImGui::BulletText("XO-CHIP");
        ImGui::Separator();

        const int sdl = SDL_GetVersion();
        ImGui::TextDisabled("Dear ImGui %s, SDL %d.%d.%d", IMGUI_VERSION,
            SDL_VERSIONNUM_MAJOR(sdl), SDL_VERSIONNUM_MINOR(sdl), SDL_VERSIONNUM_MICRO(sdl));
    }
    ImGui::End();
}

} // namespace c8
