#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <imgui.h>

namespace c8 {

class Debugger;

/// An action that a menu, a button, or a keyboard shortcut can request
enum class Command : uint8_t {
    // Files
    open_rom,
    open_rom_file,     ///< Open the ROM that UiRequests::file names, for example a recent file
    quit,

    // The assembly source in the code editor
    new_source,
    open_source,
    open_source_file,  ///< Open the source that UiRequests::file names
    disassemble,       ///< Replace the text with the disassembly of the loaded program
    save_source,
    save_source_as,
    export_rom,
    assemble_and_load,
    check_source,

    // Run control
    run_pause,
    frame_advance,
    step_into,
    step_over,
    step_out,
    run_to_cursor,     ///< Run until the PC reaches the cursor line in the code editor
    toggle_breakpoint, ///< Toggle the breakpoint of the cursor line in the code editor
    reset,

    // History
    step_back,         ///< Go back one instruction
    frame_back,        ///< Go back to the previous frame end

    // Save states
    quick_save,        ///< Save the state to the selected slot of the program
    quick_load,        ///< Load the state from the selected slot of the program
    save_state_as,
    load_state,
    open_state_file,   ///< Load the save state that UiRequests::file names

    // The answers to the prompt about unsaved changes
    prompt_save,
    prompt_discard,
    prompt_cancel,
};

/// The number of commands
inline constexpr size_t command_count = static_cast<size_t>(Command::prompt_cancel) + 1;


/// What a command is, for the menus, the buttons, the shortcuts, and the Keyboard Shortcuts window
struct CommandInfo {
    Command          command;
    std::string_view name;        ///< The text of a menu item or a button, for example "Step Over"
    std::string_view category;    ///< The group in the Keyboard Shortcuts window, for example "Debug"
    ImGuiKeyChord    chord;       ///< ImGuiKey_None if the command has no shortcut
    ImGuiInputFlags  flags;       ///< ImGuiInputFlags_Repeat repeats the command while the key is held
    std::string_view description; ///< The tooltip of a menu item or a button. Empty if the name is enough.
};

/// All commands, in the order of the enumeration
[[nodiscard]]
auto all_commands() noexcept -> std::span<const CommandInfo, command_count>;

[[nodiscard]]
auto command_info(Command command) noexcept -> const CommandInfo&;


/// True if a command replaces the source text or ends the app. With unsaved changes, the app asks first.
[[nodiscard]]
auto discards_source(Command command) noexcept -> bool;

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


/// While this key is held, the emulation runs faster
inline constexpr ImGuiKey turbo_key = ImGuiKey_Tab;

/// While this key is held, the history plays backward
inline constexpr ImGuiKey rewind_key = ImGuiKey_Backspace;


/**
 * @brief Add the commands whose shortcuts were pressed in this frame
 *
 * @details The shortcuts work in all windows, also while a text field has
 *          focus. Call once in each frame, between ImGui::NewFrame() and
 *          ImGui::Render().
 */
auto poll_shortcuts(std::vector<Command>& commands) -> void;

/**
 * @brief True while the rewind key is held
 *
 * @details Rewind is off while a text field or the code editor uses the
 *          keyboard. Call once in each frame, between ImGui::NewFrame() and
 *          ImGui::Render().
 */
[[nodiscard]]
auto poll_rewind() -> bool;

/**
 * @brief True while the turbo key is held
 *
 * @details Turbo is off while a text field or the code editor uses the
 *          keyboard. At other times, the app takes the key from ImGui, so
 *          Tab does not move the focus into a text field. Call once in each
 *          frame, between ImGui::NewFrame() and ImGui::Render().
 */
[[nodiscard]]
auto poll_turbo() -> bool;

} // namespace c8
