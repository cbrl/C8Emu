#include "commands.h"

#include "debugger/debugger.h"

#include <algorithm>
#include <array>

#include <imgui_internal.h> // GetKeyChordName, SetKeyOwner

namespace c8 {

namespace {

struct Binding {
    Command         command;
    ImGuiKeyChord   chord;
    ImGuiInputFlags flags; // ImGuiInputFlags_Repeat repeats the command while the key is held
};

constexpr auto bindings = std::array{
    Binding{Command::run_pause,         ImGuiKey_F5,                                 ImGuiInputFlags_None},
    Binding{Command::frame_advance,     ImGuiKey_F6,                                 ImGuiInputFlags_Repeat},
    Binding{Command::frame_back,        ImGuiMod_Shift | ImGuiKey_F6,                ImGuiInputFlags_Repeat},
    Binding{Command::step_back,         ImGuiKey_F7,                                 ImGuiInputFlags_Repeat},
    Binding{Command::quick_save,        ImGuiKey_F2,                                 ImGuiInputFlags_None},
    Binding{Command::quick_load,        ImGuiKey_F4,                                 ImGuiInputFlags_None},
    Binding{Command::step_over,         ImGuiKey_F10,                                ImGuiInputFlags_Repeat},
    Binding{Command::step_into,         ImGuiKey_F11,                                ImGuiInputFlags_Repeat},
    Binding{Command::step_out,          ImGuiMod_Shift | ImGuiKey_F11,               ImGuiInputFlags_None},
    Binding{Command::toggle_breakpoint, ImGuiKey_F9,                                 ImGuiInputFlags_None},
    Binding{Command::run_to_cursor,     ImGuiMod_Ctrl | ImGuiKey_F10,                ImGuiInputFlags_None},
    Binding{Command::reset,             ImGuiMod_Ctrl | ImGuiKey_R,                  ImGuiInputFlags_None},
    Binding{Command::assemble_and_load, ImGuiMod_Ctrl | ImGuiKey_B,                  ImGuiInputFlags_None},
    Binding{Command::save_source,       ImGuiMod_Ctrl | ImGuiKey_S,                  ImGuiInputFlags_None},
    Binding{Command::save_source_as,    ImGuiMod_Ctrl | ImGuiMod_Shift | ImGuiKey_S, ImGuiInputFlags_None},
};

constexpr auto turbo_key = ImGuiKey_Tab;
constexpr auto rewind_key = ImGuiKey_Backspace;

} // namespace


auto discards_source(Command command) noexcept -> bool {
    switch (command) {
        case Command::open_rom:    // The editor then shows the disassembly of the ROM
        case Command::open_rom_file:
        case Command::open_source_file:
        case Command::quit:
        case Command::new_source:
        case Command::open_source:
        case Command::disassemble: {
            return true;
        }
        default: {
            return false;
        }
    }
}


auto shortcut(Command command) noexcept -> ImGuiKeyChord {
    const auto it = std::ranges::find(bindings, command, &Binding::command);
    return (it != bindings.end()) ? it->chord : ImGuiKey_None;
}


auto shortcut_text(Command command) -> std::string {
    const auto chord = shortcut(command);
    if (chord == ImGuiKey_None) {
        return {};
    }

    // ImGui writes the name to a shared buffer, so copy it
    return std::string{ImGui::GetKeyChordName(chord)};
}


auto is_enabled(Command command, const Debugger& debugger) noexcept -> bool {
    const bool has_program = debugger.has_program();

    switch (command) {
        case Command::open_rom:
        case Command::quit:
        case Command::new_source:
        case Command::open_source:
        case Command::save_source:
        case Command::save_source_as:
        case Command::export_rom:
        case Command::assemble_and_load:
        case Command::check_source:
        case Command::toggle_breakpoint:
        case Command::load_state:
        case Command::open_rom_file:
        case Command::open_source_file:
        case Command::prompt_save:
        case Command::prompt_discard:
        case Command::prompt_cancel: {
            return true;
        }
        case Command::reset:
        case Command::run_pause:
        case Command::frame_advance:
        case Command::run_to_cursor:
        case Command::disassemble:
        case Command::step_back:
        case Command::frame_back:
        case Command::quick_save:
        case Command::quick_load:
        case Command::save_state_as: {
            return has_program;
        }
        case Command::step_into:
        case Command::step_over:
        case Command::step_out: {
            return has_program && !debugger.is_running();
        }
    }
    return false;
}


auto poll_shortcuts(std::vector<Command>& commands) -> void {
    for (const auto& binding : bindings) {
        // A global route works in each window, unless the focused window uses the same chord
        if (ImGui::Shortcut(binding.chord, binding.flags | ImGuiInputFlags_RouteGlobal)) {
            commands.push_back(binding.command);
        }
    }
}


auto poll_rewind() -> bool {
    // A text field or the code editor uses Backspace
    if (ImGui::GetIO().WantCaptureKeyboard) {
        return false;
    }
    return ImGui::IsKeyDown(rewind_key);
}


auto poll_turbo() -> bool {
    // A text field or the code editor uses Tab
    if (ImGui::GetIO().WantCaptureKeyboard) {
        return false;
    }

    // Without an owner, Tab moves the focus to the next text field. ImGui then
    // takes the keyboard, and the CHIP-8 keys stop working.
    ImGui::SetKeyOwner(turbo_key, ImGui::GetID("TurboKey"));

    return ImGui::IsKeyDown(turbo_key);
}

} // namespace c8
