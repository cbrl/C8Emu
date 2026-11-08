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
    Binding{Command::run_pause,     ImGuiKey_F5,                   ImGuiInputFlags_None},
    Binding{Command::frame_advance, ImGuiKey_F6,                   ImGuiInputFlags_Repeat},
    Binding{Command::step_over,     ImGuiKey_F10,                  ImGuiInputFlags_Repeat},
    Binding{Command::step_into,     ImGuiKey_F11,                  ImGuiInputFlags_Repeat},
    Binding{Command::step_out,      ImGuiMod_Shift | ImGuiKey_F11, ImGuiInputFlags_None},
    Binding{Command::reset,         ImGuiMod_Ctrl | ImGuiKey_R,    ImGuiInputFlags_None},
};

constexpr auto turbo_key = ImGuiKey_Tab;

} // namespace


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
        case Command::quit: {
            return true;
        }
        case Command::reset:
        case Command::run_pause:
        case Command::frame_advance: {
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
