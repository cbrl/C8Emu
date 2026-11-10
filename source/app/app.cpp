#include "app.h"

#include "app/settings_store.h"
#include "assembler/disassembler.h"
#include "chip8/serialize.h"
#include "debugger/breakpoint_remap.h"
#include "frontend/rom_files.h"
#include "util/log.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <format>
#include <string>
#include <system_error>
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

// The time from the last change of a setting to the write of the settings file.
// A drag of a slider changes a setting in each frame, but the file is written once.
constexpr auto settings_write_delay = std::chrono::seconds{1};

// The largest save state file. A state holds at most 64 KiB of memory, a program of the same size, and the display.
constexpr size_t max_state_size = 1024 * 1024;

// The absolute path of a file as UTF-8, for a list of recent files
auto recent_entry(const std::filesystem::path& file) -> std::string {
    auto ec = std::error_code{};
    const auto absolute = std::filesystem::absolute(file, ec);
    return path_to_utf8(ec ? file : absolute);
}

// Remove a file that does not exist now from a list of recent files
auto forget_missing(std::vector<std::string>& list, const std::filesystem::path& file) -> void {
    auto ec = std::error_code{};
    if (!std::filesystem::exists(file, ec)) {
        std::erase(list, recent_entry(file));
    }
}

} // namespace


App::App() {
    log_info("C8Emu started. User data folder: {}", path_to_utf8(platform.user_data_folder()));

    // Read the settings now, so they are ready before the first frame
    if (auto loaded = read_settings_file(settings_file())) {
        settings = std::move(*loaded);
    }
    else {
        log_warning("C8Emu cannot read the settings and uses the defaults. A change of a setting replaces the file. {}", loaded.error());
    }
    last_settings = settings;

    // The machine of the last session
    const auto machine = Preset{
        .id      = "saved",
        .name    = "saved",
        .variant = settings.variant,
        .quirks  = settings.quirks,
        .instructions_per_frame = settings.instructions_per_frame,
    };
    (void)debugger.apply_preset(machine);
    debugger.set_break_on_leave_program(settings.break_on_leave_program);
    sync_settings();
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

        // The shortcuts, the turbo key, and the rewind key need the key state of this ImGui frame
        auto requests = UiRequests{};
        poll_shortcuts(requests.commands);
        turbo = poll_turbo();
        rewinding = poll_rewind() && debugger.history_enabled() && debugger.has_program();

        auto context = UiContext{
            .chip            = chip,
            .debugger        = debugger,
            .session         = session,
            .document        = document,
            .settings        = settings,
            .rate            = rate_meter.rate(),
            .user_data       = platform.user_data_folder(),
            .display_texture = static_cast<ImTextureID>(display_texture.id()),
            .turbo_active    = turbo,
            .rewinding       = rewinding,
            .unsaved_prompt  = unsaved_prompt,
            .requests        = requests,
        };

        ui.draw(context);
        platform.end_frame();

        // A replay from the history would not give the edited values
        if (requests.state_edited) {
            debugger.state_edited();
        }

        for (const auto command : requests.commands) {
            if ((command == Command::open_rom_file) || (command == Command::open_source_file)) {
                pending_file = requests.file;
            }
            execute(command);
        }

        sync_settings();
        platform.limit_frame_rate(settings.frame_limit_enabled, settings.frame_limit);
    }

    // The changes of the last second are not in the file yet
    if (settings_unsaved) {
        save_settings();
    }
}


auto App::apply_input(const PlatformEvents& events) -> void {
    // The debugger notes each change, so a replay of the history presses the same keys
    if (events.release_all_keys) {
        for (uint8_t key = 0; key < 16; ++key) {
            debugger.set_key(key, false);
        }
    }

    for (const auto& event : events.keys) {
        debugger.set_key(event.key, event.down);
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

    if (chosen) {
        open_rom(*chosen);
    }
}


auto App::open_rom(const std::filesystem::path& file) -> void {
    if (!session.open_rom(file, settings)) {
        forget_missing(settings.recent_roms, file);
        return;
    }

    Settings::push_recent(settings.recent_roms, recent_entry(file));
    show_disassembly();
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
    if (chosen) {
        open_source(*chosen);
    }
}


auto App::open_source(const std::filesystem::path& file) -> void {
    const auto text = read_text_file(file);
    if (!text) {
        log_error("Cannot open the source: {}", text.error());
        forget_missing(settings.recent_sources, file);
        return;
    }

    document.replace(*text, file);
    Settings::push_recent(settings.recent_sources, recent_entry(file));
    log_info("Opened \"{}\"", path_to_utf8(file));
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
    Settings::push_recent(settings.recent_sources, recent_entry(file));
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


auto App::quick_state_file() const -> std::filesystem::path {
    return state_slot_file(state_folder(platform.user_data_folder()), session.program_name(), settings.state_slot);
}


auto App::save_state_to(const std::filesystem::path& file) -> void {
    auto ec = std::error_code{};
    std::filesystem::create_directories(file.parent_path(), ec);

    auto save = chip.save_state();
    save.name = session.program_name();

    if (const auto written = write_file(file, serialize_save_state(save)); !written) {
        log_error("Cannot save the state: {}", written.error());
        return;
    }
    log_info("Saved the state to \"{}\"", path_to_utf8(file));
}


auto App::load_state_from(const std::filesystem::path& file) -> void {
    const auto bytes = read_rom_file(file, max_state_size);
    if (!bytes) {
        log_error("Cannot load the state: {}", bytes.error());
        return;
    }

    auto save = deserialize_save_state(*bytes);
    if (!save) {
        log_error("Cannot load the state \"{}\": {}", path_to_utf8(file.filename()), save.error());
        return;
    }

    const bool same_program = (save->origin == chip.program_origin()) && std::ranges::equal(save->program, chip.program());
    const auto name = save->name;

    if (const auto loaded = debugger.load_state(std::move(*save)); !loaded) {
        log_error("Cannot load the state \"{}\": {}", path_to_utf8(file.filename()), loaded.error());
        return;
    }
    session.state_loaded(name, same_program);

    // The lines of the code editor have the addresses of the old program
    if (!same_program) {
        document.forget_program();
        debugger.set_symbols({});
        log_warning("The state holds another program, so the Code Editor shows no addresses now");
    }
    log_info("Loaded the state from \"{}\"", path_to_utf8(file));
}


auto App::save_state_dialog() -> void {
    const auto folder = state_folder(platform.user_data_folder());
    auto ec = std::error_code{};
    std::filesystem::create_directories(folder, ec);

    const auto stem = path_to_utf8(path_from_utf8(session.program_name()).stem());
    const auto name = (stem.empty() ? std::string{"state"} : stem) + std::string{state_file_extension};

    const auto chosen = show_dialog([&] {
        return dialogs.save_file(platform.window(), FileKind::state, folder, name);
    });
    if (chosen) {
        save_state_to(*chosen);
    }
}


auto App::load_state_dialog() -> void {
    const auto chosen = show_dialog([this] {
        return dialogs.open_file(platform.window(), FileKind::state, state_folder(platform.user_data_folder()));
    });
    if (chosen) {
        load_state_from(*chosen);
    }
}


auto App::go_back(Command command) -> void {
    const bool moved = (command == Command::step_back) ? debugger.step_back() : debugger.frame_back();
    if (moved) {
        return;
    }

    const auto* name = (command == Command::step_back) ? "Step Back" : "Frame Back";
    if (!debugger.history_enabled()) {
        log_info("{} needs the history. Turn on Record History in the Settings panel.", name);
    }
    else {
        log_info("{}: the history has no earlier state", name);
    }
}


auto App::sync_settings() -> void {
    debugger.set_history_enabled(settings.history_enabled);
    const size_t history_frames = size_t{settings.history_seconds} * Chip8::frame_rate;
    if (debugger.history().capacity() != history_frames) {
        debugger.set_history_length(history_frames);
    }

    // The machine of this session, for the next session
    settings.variant = chip.variant();
    settings.quirks = chip.quirks();
    settings.instructions_per_frame = chip.instructions_per_frame();
    settings.break_on_leave_program = debugger.break_on_leave_program();

    // The next open of the ROM uses the configuration that the user chose for it
    if (const auto change = session.take_profile_change()) {
        settings.rom_profiles[change->first] = change->second;
    }

    const auto now = std::chrono::steady_clock::now();
    if (settings != last_settings) {
        last_settings = settings;
        settings_changed_at = now;
        settings_unsaved = true;
    }
    if (settings_unsaved && ((now - settings_changed_at) >= settings_write_delay)) {
        save_settings();
    }
}


auto App::save_settings() -> void {
    settings_unsaved = false;
    if (const auto written = write_settings_file(settings_file(), settings); !written) {
        log_error("Cannot save the settings: {}", written.error());
    }
}


auto App::settings_file() const -> std::filesystem::path {
    return platform.user_data_folder() / settings_file_name;
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

    // Turbo runs more frames in the same time. The budget still limits the work.
    const uint32_t speed = turbo ? settings.turbo_factor : 1;

    // Rewind plays the history backward at the normal speed
    if (rewinding) {
        if (!rewind_active) {
            rewind_active = true;
            resume_after_rewind = debugger.is_running();
            debugger.pause();
        }

        rewind_backlog = std::min(rewind_backlog + to_emulated_frames(seconds), max_backlog);
        const auto frames = static_cast<uint32_t>(rewind_backlog);
        rewind_backlog -= frames;

        for (uint32_t frame = 0; frame < (frames * speed); ++frame) {
            if (!debugger.frame_back()) {
                break;
            }
        }

        // Each frame back is a stop. The log does not show them.
        (void)debugger.take_stop_event();
        backlog = 0.0;
        return;
    }

    if (rewind_active) {
        rewind_active = false;
        rewind_backlog = 0.0;
        if (resume_after_rewind) {
            debugger.run();
        }
        else {
            document.reveal(chip.state().pc);
        }
    }

    if (!debugger.is_running()) {
        // Do not let time build up while paused
        backlog = 0.0;
        return;
    }

    backlog = std::min(backlog + to_emulated_frames(seconds), max_backlog);

    const auto frames = static_cast<uint32_t>(backlog);
    backlog -= frames;

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
        case Command::open_rom_file: {
            open_rom(pending_file);
            break;
        }
        case Command::open_source_file: {
            open_source(pending_file);
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
        case Command::step_back:
        case Command::frame_back: {
            go_back(command);
            break;
        }
        case Command::quick_save: {
            save_state_to(quick_state_file());
            break;
        }
        case Command::quick_load: {
            const auto file = quick_state_file();
            auto ec = std::error_code{};
            if (!std::filesystem::exists(file, ec)) {
                log_info("Slot {} of \"{}\" has no state", settings.state_slot, session.program_name());
                break;
            }
            load_state_from(file);
            break;
        }
        case Command::save_state_as: {
            save_state_dialog();
            break;
        }
        case Command::load_state: {
            load_state_dialog();
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
