#pragma once

#include "chip8/chip8.h"
#include "media_layer/media_layer.h"


class Chip8Emulator {
public:

    /**
     * @copydoc chip8::load_rom
     */
    [[nodiscard]]
    bool load_rom(const std::filesystem::path& file) {
        return chip.load_rom(file);
    }

    /**
     * @brief Run the emulator update loop
     */
    void run();

private:

    /// Renders the user interface
    void render();


    // The CHIP-8 itself
    chip8 chip;

    MediaLayer media_layer;

    // The clock speed in Hz
    uint32_t clock_speed = 60;

    // The timer used to limit the execution rate
    Stopwatch<> clock_timer;
};