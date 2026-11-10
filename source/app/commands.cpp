#include "commands.h"

#include "debugger/debugger.h"

#include <array>

#include <imgui_internal.h> // GetKeyChordName, SetKeyOwner

namespace c8 {

namespace {

constexpr auto none = ImGuiInputFlags_None;
constexpr auto repeat = ImGuiInputFlags_Repeat;
constexpr auto no_key = ImGuiKeyChord{ImGuiKey_None};

constexpr auto ctrl = ImGuiKeyChord{ImGuiMod_Ctrl};
constexpr auto shift = ImGuiKeyChord{ImGuiMod_Shift};

// The order is the order of the enumeration, so a command is an index into the table
constexpr auto commands = [] {
    using enum Command;
    return std::array<CommandInfo, command_count>{{
        {open_rom,          "Open ROM...",         "File",    ctrl | ImGuiKey_O,         none,   {}},
        {open_rom_file,     "Open Recent ROM",     "File",    no_key,                    none,   {}},
        {quit,              "Exit",                "File",    no_key,                    none,   {}},

        {new_source,        "New Source",          "Source",  no_key,                    none,   {}},
        {open_source,       "Open Source...",      "Source",  ctrl | shift | ImGuiKey_O, none,   {}},
        {open_source_file,  "Open Recent Source",  "Source",  no_key,                    none,   {}},
        {disassemble,       "Disassemble Program", "Source",  no_key,                    none,   {}},
        {save_source,       "Save Source",         "Source",  ctrl | ImGuiKey_S,         none,   {}},
        {save_source_as,    "Save Source As...",   "Source",  ctrl | shift | ImGuiKey_S, none,   {}},
        {export_rom,        "Export ROM...",       "Source",  ctrl | ImGuiKey_E,         none,   "Assemble the source and write the program to a ROM file"},
        {assemble_and_load, "Assemble & Load",     "Source",  ctrl | ImGuiKey_B,         none,   {}},
        {check_source,      "Check Source",        "Source",  ctrl | shift | ImGuiKey_B, none,   "Assemble the source and show the problems, but do not load it"},

        {run_pause,         "Run / Pause",         "Debug",   ImGuiKey_F5,               none,   {}},
        {frame_advance,     "Frame Advance",       "Debug",   ImGuiKey_F6,               repeat, "Run to the end of the frame, then pause"},
        {step_into,         "Step Into",           "Debug",   ImGuiKey_F11,              repeat, {}},
        {step_over,         "Step Over",           "Debug",   ImGuiKey_F10,              repeat, {}},
        {step_out,          "Step Out",            "Debug",   shift | ImGuiKey_F11,      none,   {}},
        {run_to_cursor,     "Run to Cursor",       "Debug",   ctrl | ImGuiKey_F10,       none,   "Run until the PC reaches the cursor line in the Code Editor"},
        {toggle_breakpoint, "Toggle Breakpoint",   "Debug",   ImGuiKey_F9,               none,   "Toggle the breakpoint of the cursor line in the Code Editor"},
        {reset,             "Reset",               "Debug",   ctrl | ImGuiKey_R,         none,   {}},

        {step_back,         "Step Back",           "History", ImGuiKey_F7,               repeat, "Go back one instruction"},
        {frame_back,        "Frame Back",          "History", shift | ImGuiKey_F6,       repeat, "Go back to the end of the previous frame"},

        {quick_save,        "Quick Save State",    "State",   ImGuiKey_F2,               none,   "Save the state to the selected slot"},
        {quick_load,        "Quick Load State",    "State",   ImGuiKey_F4,               none,   "Load the state from the selected slot"},
        {save_state_as,     "Save State As...",    "State",   no_key,                    none,   {}},
        {load_state,        "Load State...",       "State",   no_key,                    none,   {}},
        {open_state_file,   "Open State File",     "State",   no_key,                    none,   {}},

        {prompt_save,       "Save",                "Prompt",  no_key,                    none,   {}},
        {prompt_discard,    "Discard",             "Prompt",  no_key,                    none,   {}},
        {prompt_cancel,     "Cancel",              "Prompt",  no_key,                    none,   {}},
    }};
}();

constexpr auto in_enum_order = [] {
    for (size_t index = 0; index < commands.size(); ++index) {
        if (static_cast<size_t>(commands[index].command) != index) {
            return false;
        }
    }
    return true;
}();
static_assert(in_enum_order, "The command table must list the commands in the order of the enumeration");

} // namespace


auto all_commands() noexcept -> std::span<const CommandInfo, command_count> {
    return commands;
}


auto command_info(Command command) noexcept -> const CommandInfo& {
    return commands[static_cast<size_t>(command)];
}


auto discards_source(Command command) noexcept -> bool {
    using enum Command;
    switch (command) {
        case open_rom:    // The editor then shows the disassembly of the ROM
        case open_rom_file:
        case open_source_file:
        case quit:
        case new_source:
        case open_source:
        case disassemble: {
            return true;
        }
        default: {
            return false;
        }
    }
}


auto shortcut_text(Command command) -> std::string {
    const auto chord = command_info(command).chord;
    if (chord == ImGuiKey_None) {
        return {};
    }

    // ImGui writes the name to a shared buffer, so copy it
    return std::string{ImGui::GetKeyChordName(chord)};
}


auto is_enabled(Command command, const Debugger& debugger) noexcept -> bool {
    using enum Command;
    const bool has_program = debugger.has_program();

    switch (command) {
        case open_rom:
        case open_rom_file:
        case quit:
        case new_source:
        case open_source:
        case open_source_file:
        case save_source:
        case save_source_as:
        case export_rom:
        case assemble_and_load:
        case check_source:
        case toggle_breakpoint:
        case load_state:
        case open_state_file:
        case prompt_save:
        case prompt_discard:
        case prompt_cancel: {
            return true;
        }
        case disassemble:
        case run_pause:
        case frame_advance:
        case run_to_cursor:
        case reset:
        case step_back:
        case frame_back:
        case quick_save:
        case quick_load:
        case save_state_as: {
            return has_program;
        }
        case step_into:
        case step_over:
        case step_out: {
            return has_program && !debugger.is_running();
        }
    }
    return false;
}


auto poll_shortcuts(std::vector<Command>& requested) -> void {
    for (const auto& info : commands) {
        if (info.chord == ImGuiKey_None) {
            continue;
        }
        // A global route works in each window, unless the focused window uses the same chord
        if (ImGui::Shortcut(info.chord, info.flags | ImGuiInputFlags_RouteGlobal)) {
            requested.push_back(info.command);
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
