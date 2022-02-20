#include "chip8_emulator.h"


auto Chip8Emulator::run() -> void {
    bool stop = false;
    auto clock_dt = std::chrono::duration<double>{0};

    while (!stop) {
        // Update the timer
        timer.tick();
        clock_dt += timer.delta_time();

        // If not paused and the elapsed time is greater than the clock period, then update.
        if (clock_dt.count() >= (1.0 / chip.get_clock_rate()) and !chip.is_paused()) {
            chip.run_cycle();
            clock_dt = std::chrono::duration<double>{0};
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
