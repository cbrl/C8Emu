#include "chip8_emulator.h"


void Chip8Emulator::run() {
    bool stop = false;
    std::chrono::duration<double> delta_time{0};

    while (!stop) {
        // Update the clock timer
        clock_timer.tick();
        delta_time += clock_timer.delta_time();

        // If the elapsed time is greater than the clock period, then update.
        if (delta_time.count() >= (1.0 / clock_speed)) {
            chip.run_cycle();
            delta_time = std::chrono::duration<double>{0};
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