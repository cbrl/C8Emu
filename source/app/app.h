#pragma once

#include "app/commands.h"
#include "app/session.h"
#include "app/source_document.h"
#include "chip8/chip8.h"
#include "debugger/debugger.h"
#include "frontend/rate_meter.h"
#include "frontend/settings.h"
#include "platform/beeper.h"
#include "platform/display_texture.h"
#include "platform/file_dialogs.h"
#include "platform/platform.h"
#include "ui/ui.h"
#include "util/stopwatch/stopwatch.h"

#include <array>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <optional>

namespace c8 {

/**
 * @class App
 *
 * @brief The emulator application and its main loop
 */
class App {
    struct AudioPattern {
        bool loaded = false;
        uint8_t pitch = MachineState::default_pitch;
        std::array<uint8_t, 16> pattern{};

        friend auto operator==(const AudioPattern&, const AudioPattern&) -> bool = default;
    };

public:
    App();

    /// Run the main loop until the user quits
    auto run() -> void;

private:
    // Apply the CHIP-8 key changes of one host frame
    auto apply_input(const PlatformEvents& events) -> void;

    // Set the keys that the mouse holds on the Keypad panel
    auto set_keypad_keys(uint16_t keys) -> void;

    // Give the debugger the keys that the keyboard or the Keypad panel holds
    auto update_keys() -> void;

    // Open a file that the user dropped onto the window: a source, a save state, or a ROM
    auto open_dropped_file(const std::filesystem::path& file) -> void;

    // Send the XO-CHIP audio pattern and pitch to the beeper when they change
    auto update_audio_pattern() -> void;

    // Ask the user for a ROM file, then load it and show its disassembly
    auto open_rom_dialog() -> void;

    // Load a ROM file and show its disassembly. The file goes to the front of the recent ROMs.
    auto open_rom(const std::filesystem::path& file) -> void;

    // Show the disassembly of the loaded program in the code editor
    auto show_disassembly() -> void;

    // Ask the user for a source file, then show it in the code editor
    auto open_source_dialog() -> void;

    // Show a source file in the code editor. The file goes to the front of the recent sources.
    auto open_source(const std::filesystem::path& file) -> void;

    // Save the source text. Ask for a file if the text has none or if choose_file is true.
    // Returns false if the text is not saved.
    auto save_source(bool choose_file) -> bool;

    // Assemble the source text, then ask for a file and write the program to it
    auto export_rom_dialog() -> void;

    // Assemble the source text and load the program if it has no errors. The breakpoints move with their lines.
    auto assemble_and_load() -> void;

    // The program bytes of the cursor line in the code editor. Logs the reason if the line has none.
    auto cursor_line_bytes() -> std::optional<LineBytes>;

    // Assemble the source text and write a summary of the problems to the log
    auto check_source() -> void;

    // The file of the selected quick save slot of the program
    [[nodiscard]]
    auto quick_state_file() const -> std::filesystem::path;

    // Write the machine to a save state file
    auto save_state_to(const std::filesystem::path& file) -> void;

    // Restore the machine from a save state file
    auto load_state_from(const std::filesystem::path& file) -> void;

    // Ask the user for a save state file, then write the machine to it
    auto save_state_dialog() -> void;

    // Ask the user for a save state file, then restore the machine from it
    auto load_state_dialog() -> void;

    // Step back or frame back. Logs the reason if the history has no earlier state.
    auto go_back(Command command) -> void;

    // Give the settings to the debugger, copy the machine configuration into the settings,
    // and write the settings file a short time after a change
    auto sync_settings() -> void;

    // Write the settings file now
    auto save_settings() -> void;

    // The settings file in the user data folder
    [[nodiscard]]
    auto settings_file() const -> std::filesystem::path;

    // Show a file dialog. Returns the chosen file, or std::nullopt if the user cancelled or the dialog failed.
    auto show_dialog(const std::function<FileDialogs::Result()>& show) -> std::optional<std::filesystem::path>;

    // The folder that the source and export dialogs show first
    [[nodiscard]]
    auto source_folder() const -> std::filesystem::path;

    // Run the emulated frames that are due after `seconds` of host time. While the
    // rewind key is held, go back one recorded frame for each emulated frame instead.
    auto emulate(double seconds) -> void;

    // Write each stop of execution to the log
    auto log_stop_events() -> void;

    // Write the messages of the logpoints to the log
    auto log_logpoint_messages() -> void;

    // Do a command from a menu, a button, or a shortcut. A command that discards
    // unsaved source changes waits for an answer to a prompt first.
    auto execute(Command command) -> void;

    // Do a command without the checks of execute()
    auto perform(Command command) -> void;

    Settings settings;
    Keymap keymap = default_keymap();

    // The CHIP-8 keys that the keyboard holds, and the keys that the mouse holds on the Keypad panel.
    // The machine sees both. Bit n is key n.
    uint16_t keyboard_keys = 0;
    uint16_t keypad_keys = 0;

    Chip8 chip;
    Debugger debugger{chip};
    Session session{chip, debugger};

    // The members after the platform need its SDL, OpenGL, and ImGui state.
    // They are destroyed before it.
    Platform platform;
    Beeper beeper;
    DisplayTexture display_texture;
    FileDialogs dialogs;
    SourceDocument document;
    Ui ui;

    RateMeter rate_meter;
    Stopwatch<> timer;

    // Emulated frames that are due but did not run yet
    double backlog = 0.0;

    // The turbo key was held in the last frame
    bool turbo = false;

    // The rewind key was held in the last frame, and the history can play backward
    bool rewinding = false;

    // The rewind started in the last frames. It paused the program, if the program ran.
    bool rewind_active = false;
    bool resume_after_rewind = false;
    double rewind_backlog = 0.0;

    // The settings at the end of the last frame. A difference from `settings` is a change.
    Settings last_settings;

    // The settings changed at this time, and the settings file does not have the change yet
    bool settings_unsaved = false;
    std::chrono::steady_clock::time_point settings_changed_at;

    // The file of a request to open a file. It waits with the command for an answer about unsaved changes.
    std::filesystem::path pending_file;

    // The keyboard key of each CHIP-8 key, for the UI
    std::array<std::string, 16> keymap_names;

    // The XO-CHIP sound that the beeper plays
    AudioPattern audio_pattern;

    // A command that waits until the user saves or discards the source changes
    std::optional<Command> unsaved_prompt;

    bool quit = false;
};

} // namespace c8
