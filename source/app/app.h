#pragma once

#include "app/commands.h"
#include "app/session.h"
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

    // Ask the user for a ROM file, then load it
    auto open_rom_dialog() -> void;

    // Run the emulated frames that are due after `seconds` of host time
    auto emulate(double seconds) -> void;

    // Write each stop of execution to the log
    auto log_stop_events() -> void;

    // Do a command from a menu, a button, or a shortcut
    auto execute(Command command) -> void;

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
    Ui ui;

    RateMeter rate_meter;
    Stopwatch<> timer;

    // Emulated frames that are due but did not run yet
    double backlog = 0.0;

    // The turbo key was held in the last frame
    bool turbo = false;

    bool quit = false;
};

} // namespace c8
