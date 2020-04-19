#include "chip8_emulator.h"


void Chip8Emulator::run() {
    bool stop = false;
    std::chrono::duration<double> clock_dt{0};

    while (!stop) {
        // Update the timer
        timer.tick();
        clock_dt += timer.delta_time();

        // If the elapsed time is greater than the clock period, then update.
        if (clock_dt.count() >= (1.0 / chip.get_clock_rate())) {
            chip.run_cycle();
            clock_dt = std::chrono::duration<double>{0};
        }

        // Process SDL events
        media_layer.process_events(chip, stop);

        // Render the UI
        render();
    }
}


void Chip8Emulator::render() {
    media_layer.render(chip);
}