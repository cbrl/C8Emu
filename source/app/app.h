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
public:
    App();

    /// Run the main loop until the user quits
    auto run() -> void;

private:
    // Apply the CHIP-8 key changes of one host frame
    auto apply_input(const PlatformEvents& events) -> void;

    // Ask the user for a ROM file, then load it and show its disassembly
    auto open_rom_dialog() -> void;

    // Show the disassembly of the loaded program in the code editor
    auto show_disassembly() -> void;

    // Ask the user for a source file, then show it in the code editor
    auto open_source_dialog() -> void;

    // Save the source text. Ask for a file if the text has none or if choose_file is true.
    // Returns false if the text is not saved.
    auto save_source(bool choose_file) -> bool;

    // Assemble the source text, then ask for a file and write the program to it
    auto export_rom_dialog() -> void;

    // Assemble the source text and load the program if it has no errors
    auto assemble_and_load() -> void;

    // Assemble the source text and write a summary of the problems to the log
    auto check_source() -> void;

    // Show a file dialog. Returns the chosen file, or std::nullopt if the user cancelled or the dialog failed.
    auto show_dialog(const std::function<FileDialogs::Result()>& show) -> std::optional<std::filesystem::path>;

    // The folder that the source and export dialogs show first
    [[nodiscard]]
    auto source_folder() const -> std::filesystem::path;

    // Run the emulated frames that are due after `seconds` of host time
    auto emulate(double seconds) -> void;

    // Write each stop of execution to the log
    auto log_stop_events() -> void;

    // Do a command from a menu, a button, or a shortcut. A command that discards
    // unsaved source changes waits for an answer to a prompt first.
    auto execute(Command command) -> void;

    // Do a command without the checks of execute()
    auto perform(Command command) -> void;

    Settings settings;
    Keymap keymap = default_keymap();

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

    // A command that waits until the user saves or discards the source changes
    std::optional<Command> unsaved_prompt;

    bool quit = false;
};

} // namespace c8
