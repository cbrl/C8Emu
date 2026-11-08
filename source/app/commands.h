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
};


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
