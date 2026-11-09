#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include <imgui.h>

namespace c8 {

class Debugger;

/// An action that a menu, a button, or a keyboard shortcut can request
enum class Command : uint8_t {
    open_rom,
    quit,
    reset,
    run_pause,
    step_into,
    step_over,
    step_out,
    frame_advance,
    toggle_breakpoint, ///< Toggle the breakpoint of the cursor line in the code editor
    run_to_cursor,     ///< Run until the PC reaches the cursor line in the code editor

    // The assembly source in the code editor
    new_source,
    open_source,
    disassemble, ///< Replace the text with the disassembly of the loaded program
    save_source,
    save_source_as,
    export_rom,
    assemble_and_load,
    check_source,

    // The answers to the prompt about unsaved changes
    prompt_save,
    prompt_discard,
    prompt_cancel,
};


/// True if a command replaces the source text or ends the app. With unsaved changes, the app asks first.
[[nodiscard]]
auto discards_source(Command command) noexcept -> bool;


/// The keyboard shortcut of a command. ImGuiKey_None if the command has no shortcut.
[[nodiscard]]
auto shortcut(Command command) noexcept -> ImGuiKeyChord;

/// The shortcut as text for a menu, for example "Shift+F11". Empty if the command has no shortcut.
[[nodiscard]]
auto shortcut_text(Command command) -> std::string;

/**
 * @brief True if the command can run now
 *
 * @details The step commands need a paused program. Frame advance also works
 *          while the program runs: it pauses at the end of the frame.
 */
[[nodiscard]]
auto is_enabled(Command command, const Debugger& debugger) noexcept -> bool;


/**
 * @brief Add the commands whose shortcuts were pressed in this frame
 *
 * @details The shortcuts work in all windows, also while a text field has
 *          focus. Call once in each frame, between ImGui::NewFrame() and
 *          ImGui::Render().
 */
auto poll_shortcuts(std::vector<Command>& commands) -> void;

/**
 * @brief True while the turbo key (Tab) is held
 *
 * @details Turbo is off while a text field or the code editor uses the
 *          keyboard. At other times, the app takes the Tab key from ImGui, so
 *          Tab does not move the focus into a text field. Call once in each
 *          frame, between ImGui::NewFrame() and ImGui::Render().
 */
[[nodiscard]]
auto poll_turbo() -> bool;

} // namespace c8
