#include "app.h"

#include "assembler/disassembler.h"
#include "debugger/breakpoint_remap.h"
#include "frontend/rom_files.h"
#include "util/log.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <format>
#include <string>
#include <utility>

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

// A count and a noun, for example "1 error" or "2 errors"
auto count_of(size_t count, std::string_view noun) -> std::string {
    return std::format("{} {}{}", count, noun, (count == 1) ? "" : "s");
}

// The largest source file that the editor opens
constexpr size_t max_source_size = 4 * 1024 * 1024;

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
            execute(Command::quit);
        }
        apply_input(events);

        emulate(timer.delta_time().count());
        log_stop_events();
        log_logpoint_messages();

        // Beep only while the program runs, so a pause during a beep is silent
        beeper.set_active(debugger.is_running() && chip.sound_active());
        beeper.set_volume(settings.muted ? 0.0f : settings.volume);
        update_audio_pattern();

        // A text without a platform line uses the platform of the machine
        document.set_default_platform(chip.variant());

        rate_meter.update(chip.state(), RateMeter::clock::now());
        display_texture.update(chip.state().display, settings.palette);

        platform.begin_frame();

        // The shortcuts and the turbo key need the key state of this ImGui frame
        auto requests = UiRequests{};
        poll_shortcuts(requests.commands);
        turbo = poll_turbo();

        auto context = UiContext{
            .chip            = chip,
            .debugger        = debugger,
            .session         = session,
            .document        = document,
            .settings        = settings,
            .rate            = rate_meter.rate(),
            .display_texture = static_cast<ImTextureID>(display_texture.id()),
            .turbo_active    = turbo,
            .unsaved_prompt  = unsaved_prompt,
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


auto App::update_audio_pattern() -> void {
    const auto& state = chip.state();

    const auto sound = AudioPattern{
        .loaded  = state.audio_pattern_loaded,
        .pitch   = state.pitch,
        .pattern = state.audio_pattern,
    };
    if (sound == audio_pattern) {
        return;
    }
    audio_pattern = sound;

    // Until the program runs F002, the beep is the plain square wave
    if (sound.loaded) {
        beeper.set_pattern(sound.pattern, ToneGenerator::pitch_to_rate(sound.pitch));
    }
    else {
        beeper.clear_pattern();
    }
}


auto App::open_rom_dialog() -> void {
    const auto chosen = show_dialog([this] {
        return dialogs.open_file(platform.window(), FileKind::rom, session.browse_folder());
    });

    if (chosen && session.open_rom(*chosen, settings.auto_detect_platform)) {
        show_disassembly();
    }
}


auto App::show_disassembly() -> void {
    const auto options = DisassemblyOptions{
        .origin  = chip.program_origin(),
        .mode    = settings.disassembly_mode,
        .variant = chip.variant(),
    };

    // The check of the disassembly must use the same platform
    document.set_default_platform(chip.variant());
    const auto disassembly = disassemble(chip.program(), options);

    document.replace(disassembly.text(), {});

    // The disassembly assembles to the program in memory, so its lines have known addresses
    if (document.last_check().ok()) {
        document.mark_loaded();
    }
    else {
        log_warning("The disassembly does not assemble, so the code editor cannot show the breakpoints");
    }

    // The conditions and log messages use the labels of this program, or no labels
    debugger.set_symbols(document.program_symbols());
}


auto App::open_source_dialog() -> void {
    const auto chosen = show_dialog([this] {
        return dialogs.open_file(platform.window(), FileKind::source, source_folder());
    });
    if (!chosen) {
        return;
    }

    const auto text = read_text_file(*chosen, max_source_size);
    if (!text) {
        log_error("Cannot open the source: {}", text.error());
        return;
    }

    document.replace(*text, *chosen);
    log_info("Opened \"{}\"", path_to_utf8(*chosen));
}


auto App::save_source(bool choose_file) -> bool {
    auto file = document.path();

    if (choose_file || file.empty()) {
        const auto name = file.empty() ? std::string{"untitled.c8s"} : path_to_utf8(file.filename());
        const auto chosen = show_dialog([&] {
            return dialogs.save_file(platform.window(), FileKind::source, source_folder(), name);
        });
        if (!chosen) {
            return false;
        }
        file = *chosen;
    }

    auto text = document.text();
    if (const auto written = write_text_file(file, text); !written) {
        log_error("Cannot save the source: {}", written.error());
        return false;
    }

    document.mark_saved(file, std::move(text));
    log_info("Saved \"{}\"", path_to_utf8(file));
    return true;
}


auto App::export_rom_dialog() -> void {
    const auto& result = document.check();

    if (!result.ok()) {
        log_error("Cannot export the ROM: the source has {}. The Code Editor lists them.", count_of(result.error_count(), "error"));
        return;
    }
    if (result.image.empty()) {
        log_error("Cannot export the ROM: the source writes no bytes");
        return;
    }

    // Keep a copy, because the result belongs to the document
    const auto image = result.image;
    const auto& source = document.path();
    const auto name = source.empty() ? std::string{"untitled.ch8"} : (path_to_utf8(source.stem()) + ".ch8");

    const auto chosen = show_dialog([&] {
        return dialogs.save_file(platform.window(), FileKind::rom, source_folder(), name);
    });
    if (!chosen) {
        return;
    }

    if (const auto written = write_file(*chosen, image); !written) {
        log_error("Cannot export the ROM: {}", written.error());
        return;
    }
    log_info("Exported \"{}\" ({} bytes)", path_to_utf8(*chosen), image.size());
}


auto App::assemble_and_load() -> void {
    const auto& result = document.check();

    if (!result.ok()) {
        log_error("The assembly has {}. The Code Editor lists them.", count_of(result.error_count(), "error"));
        return;
    }
    if (result.image.empty()) {
        log_error("Nothing to load: the source writes no bytes");
        return;
    }
    if (result.warning_count() > 0) {
        log_warning("The assembly has {}. The Code Editor lists them.", count_of(result.warning_count(), "warning"));
    }

    // The breakpoints move with their lines to the new addresses
    const auto remapped = remap_breakpoints(debugger.breakpoint_addresses(), document.lines(), result.map);

    // Keep a copy, because a change of platform checks the document again
    const auto image = result.image;
    const auto variant = result.variant;
    if (!session.load_assembled(image, variant, document.display_name())) {
        return;
    }
    document.set_default_platform(chip.variant());

    debugger.move_breakpoints(remapped.moves);
    for (const auto address : remapped.dropped) {
        log_warning("Removed the breakpoint at 0x{:03X}: its line was deleted or writes no bytes now", address);
    }

    document.mark_loaded();
    debugger.set_symbols(document.program_symbols());
}


auto App::cursor_line_bytes() -> std::optional<LineBytes> {
    const auto line = document.editor().GetMainCursorPosition().line;

    auto bytes = document.bytes_of_line(line);
    if (!bytes) {
        log_warning("Code Editor line {}: {}", line + 1, bytes.error());
        return std::nullopt;
    }
    return *bytes;
}


auto App::check_source() -> void {
    const auto& result = document.check();
    const auto name = document.display_name();

    if (result.diagnostics.empty()) {
        log_info("Check of \"{}\": no problems", name);
    }
    else {
        log_info("Check of \"{}\": {} and {}", name, count_of(result.error_count(), "error"), count_of(result.warning_count(), "warning"));
    }
}


auto App::show_dialog(const std::function<FileDialogs::Result()>& show) -> std::optional<std::filesystem::path> {
    // The dialog blocks the main loop, so the beep would sound until the dialog closes
    beeper.set_active(false);

    const auto chosen = show();

    // Do not count the time in the dialog as emulated time
    timer.tick();

    if (!chosen) {
        log_error("The file dialog failed: {}", chosen.error());
        return std::nullopt;
    }
    return *chosen;
}


auto App::source_folder() const -> std::filesystem::path {
    if (!document.path().empty()) {
        return document.path().parent_path();
    }
    return session.browse_folder();
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

    // Show where the program stopped
    document.reveal(event->pc);

    const auto message = describe(*event);

    switch (event->reason) {
        case StopReason::fault:
            log_error("{}", message);
            break;
        case StopReason::left_program:
        case StopReason::exited:
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


auto App::log_logpoint_messages() -> void {
    for (const auto& message : debugger.take_log_messages()) {
        log_info("{}", message);
    }
}


auto App::execute(Command command) -> void {
    // A shortcut can request a command that the menu shows as disabled
    if (!is_enabled(command, debugger)) {
        return;
    }

    if (discards_source(command) && document.is_dirty()) {
        unsaved_prompt = command;
        return;
    }

    perform(command);
}


auto App::perform(Command command) -> void {
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
        case Command::toggle_breakpoint: {
            if (const auto bytes = cursor_line_bytes()) {
                (void)debugger.toggle_breakpoint_in(bytes->address, bytes->size);
            }
            break;
        }
        case Command::run_to_cursor: {
            if (const auto bytes = cursor_line_bytes()) {
                debugger.run_to(bytes->address);
            }
            break;
        }
        case Command::new_source: {
            document.replace("", {});
            break;
        }
        case Command::open_source: {
            open_source_dialog();
            break;
        }
        case Command::disassemble: {
            show_disassembly();
            break;
        }
        case Command::save_source: {
            (void)save_source(false);
            break;
        }
        case Command::save_source_as: {
            (void)save_source(true);
            break;
        }
        case Command::export_rom: {
            export_rom_dialog();
            break;
        }
        case Command::assemble_and_load: {
            assemble_and_load();
            break;
        }
        case Command::check_source: {
            check_source();
            break;
        }
        case Command::prompt_save: {
            // A cancelled or failed save also cancels the command
            const auto pending = std::exchange(unsaved_prompt, std::nullopt);
            if (pending && save_source(false)) {
                perform(*pending);
            }
            break;
        }
        case Command::prompt_discard: {
            const auto pending = std::exchange(unsaved_prompt, std::nullopt);
            if (pending) {
                perform(*pending);
            }
            break;
        }
        case Command::prompt_cancel: {
            unsaved_prompt.reset();
            break;
        }
    }
}

} // namespace c8
