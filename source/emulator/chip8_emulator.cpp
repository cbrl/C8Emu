#include "chip8_emulator.h"

#include "frontend/rom_files.h"

#include <chrono>


auto Chip8Emulator::load_rom(const std::filesystem::path& file) -> std::expected<void, std::string> {
    return c8::read_rom_file(file, chip.max_program_size())
        .and_then([this](const std::vector<uint8_t>& data) { return chip.load_program(data); });
}


auto Chip8Emulator::run() -> void {
    using seconds = std::chrono::duration<double>;
    static constexpr auto timer_period = seconds{1.0 / 60.0};

    bool stop = false;
    auto clock_dt = seconds{0};
    auto timer_dt = seconds{0};

    while (!stop) {
        // Update the timer
        timer.tick();
        const auto dt = timer.delta_time();

        if (chip.is_paused()) {
            // Do not let time build up while paused
            clock_dt = seconds{0};
            timer_dt = seconds{0};
        }
        else {
            clock_dt += dt;
            timer_dt += dt;

            // If the elapsed time is greater than the clock period, then run a cycle
            if (clock_dt.count() >= (1.0 / chip.get_clock_rate())) {
                chip.run_cycle();
                clock_dt = seconds{0};
            }

            // Decrement the delay and sound timers at 60 Hz. Drop the backlog after a long frame.
            if (timer_dt >= timer_period) {
                chip.tick_timers();
                timer_dt = (timer_dt >= (2 * timer_period)) ? seconds{0} : (timer_dt - timer_period);
            }
        }

        // Process SDL events
        media_layer.process_events(chip, stop);

        // Render the UI
        render();
    }
}


auto Chip8Emulator::render() -> void {
    media_layer.render(chip);
}
