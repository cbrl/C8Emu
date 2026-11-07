#include "chip8_emulator.h"

#include "frontend/rom_files.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>

namespace {

// Convert the time of one host frame to emulated frames
auto to_emulated_frames(double seconds) -> double {
	constexpr double frame_period = 1.0 / c8::Chip8::frame_rate;

    // A long stall (a breakpoint in a native debugger, a blocked window) counts as a short one
    seconds = std::clamp(seconds, 0.0, 0.25);

    // A 60 Hz frame limit jitters around the frame period. Count each such frame as
    // exactly one frame, so the jitter does not skip or double a frame.
    if (std::abs(seconds - frame_period) <= 0.001) {
        return 1.0;
    }
    return seconds * c8::Chip8::frame_rate;
}

} // namespace


auto Chip8Emulator::load_rom(const std::filesystem::path& file) -> std::expected<void, std::string> {
    return c8::read_rom_file(file, chip.max_program_size())
        .and_then([this](const std::vector<uint8_t>& data) {
			return debugger.load_program(data);
		});
}


auto Chip8Emulator::run() -> void {
    // After a slow host frame, catch up at most this many frames
    static constexpr double max_backlog = 4.0;

    // The longest time that one host frame spends in emulation
    static constexpr auto emulation_budget = std::chrono::milliseconds{12};

    // Emulated frames that are due but did not run yet
    double backlog = 0.0;
    bool quit = false;

    while (!quit) {
        timer.tick();

        // Read the input first, so the emulated frames see the newest key state
        media_layer.process_events(chip, quit);

        if (debugger.is_running()) {
            backlog = std::min(backlog + to_emulated_frames(timer.delta_time().count()), max_backlog);

            const auto frames = static_cast<uint32_t>(backlog);
            backlog -= frames;

            if (frames > 0) {
                (void)debugger.run_frames(frames, c8::Debugger::clock::now() + emulation_budget);
            }
        }
        else {
            // Do not let time build up while paused
            backlog = 0.0;
        }

        media_layer.render(chip, debugger);
    }
}
