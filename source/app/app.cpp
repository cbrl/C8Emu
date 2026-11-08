#include "app.h"

#include "util/log.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>

namespace c8 {

namespace {

// Convert the time of one host frame to emulated frames
auto to_emulated_frames(double seconds) -> double {
    constexpr double frame_period = 1.0 / Chip8::frame_rate;

    // A long stall (a breakpoint in a native debugger, a blocked window) counts as a short one
    seconds = std::clamp(seconds, 0.0, 0.25);

    // A 60 Hz frame limit jitters around the frame period. Count each such frame as
    // exactly one frame, so the jitter does not skip or double a frame.
    if (std::abs(seconds - frame_period) <= 0.001) {
        return 1.0;
    }
    return seconds * Chip8::frame_rate;
}

} // namespace


App::App() {
    log_info("C8Emu started");
}


auto App::run() -> void {
    while (!quit) {
        timer.tick();

        // Read the input first, so the emulated frames see the newest key state
        const auto events = platform.poll_events(keymap);
        if (events.quit) {
            quit = true;
        }
        apply_input(events);

        emulate(timer.delta_time().count());
        log_stop_events();

        // Beep only while the program runs, so a pause during a beep is silent
        beeper.set_active(debugger.is_running() && chip.sound_active());
        beeper.set_volume(settings.muted ? 0.0f : settings.volume);

        rate_meter.update(chip.state(), RateMeter::clock::now());
        display_texture.update(chip.state().display, settings.foreground_color, settings.background_color);

        platform.begin_frame();

        // The shortcuts and the turbo key need the key state of this ImGui frame
        auto requests = UiRequests{};
        poll_shortcuts(requests.commands);
        turbo = poll_turbo();

        auto context = UiContext{
            .chip            = chip,
            .debugger        = debugger,
            .session         = session,
            .settings        = settings,
            .rate            = rate_meter.rate(),
            .display_texture = static_cast<ImTextureID>(display_texture.id()),
            .turbo_active    = turbo,
            .requests        = requests,
        };

        ui.draw(context);
        platform.end_frame();

        for (const auto command : requests.commands) {
            execute(command);
        }
        platform.limit_frame_rate(settings.frame_limit_enabled, settings.frame_limit);
    }
}


auto App::apply_input(const PlatformEvents& events) -> void {
    if (events.release_all_keys) {
        for (uint8_t key = 0; key < 16; ++key) {
            chip.set_key(key, false);
        }
    }

    for (const auto& event : events.keys) {
        chip.set_key(event.key, event.down);
    }
}


auto App::open_rom_dialog() -> void {
    // The dialog blocks the main loop, so the beep would sound until the dialog closes
    beeper.set_active(false);

    const auto chosen = dialogs.open_rom(platform.window(), session.browse_folder());
    if (!chosen) {
        log_error("The file dialog failed: {}", chosen.error());
    }
    else if (*chosen) {
        (void)session.open_rom(**chosen);
    }

    // Do not count the time in the dialog as emulated time
    timer.tick();
}


auto App::emulate(double seconds) -> void {
    // After a slow host frame, catch up at most this many frames
    static constexpr double max_backlog = 4.0;

    // The longest time that one host frame spends in emulation
    static constexpr auto emulation_budget = std::chrono::milliseconds{12};

    if (!debugger.is_running()) {
        // Do not let time build up while paused
        backlog = 0.0;
        return;
    }

    backlog = std::min(backlog + to_emulated_frames(seconds), max_backlog);

    const auto frames = static_cast<uint32_t>(backlog);
    backlog -= frames;

    // Turbo runs more frames in the same time. The budget still limits the work.
    const uint32_t speed = turbo ? settings.turbo_factor : 1;

    if (frames > 0) {
        (void)debugger.run_frames(frames * speed, Debugger::clock::now() + emulation_budget);
    }
}


auto App::log_stop_events() -> void {
    const auto event = debugger.take_stop_event();
    if (!event) {
        return;
    }

    const auto message = describe(*event);

    switch (event->reason) {
        case StopReason::fault:
            log_error("{}", message);
            break;
        case StopReason::left_program:
            log_warning("{}", message);
            break;
        case StopReason::step:
            log_debug("{}", message);
            break;
        default:
            log_info("{}", message);
            break;
    }
}


auto App::execute(Command command) -> void {
    // A shortcut can request a command that the menu shows as disabled
    if (!is_enabled(command, debugger)) {
        return;
    }

    switch (command) {
        case Command::open_rom: {
            open_rom_dialog();
            break;
        }
        case Command::quit: {
            quit = true;
            break;
        }
        case Command::reset: {
            session.reset();
            break;
        }
        case Command::run_pause: {
            if (debugger.is_running()) {
                debugger.pause();
            }
            else {
                debugger.run();
            }
            break;
        }
        case Command::step_into: {
            debugger.step_into();
            break;
        }
        case Command::step_over: {
            debugger.step_over();
            break;
        }
        case Command::step_out: {
            debugger.step_out();
            break;
        }
        case Command::frame_advance: {
            debugger.frame_advance();
            break;
        }
    }
}

} // namespace c8
