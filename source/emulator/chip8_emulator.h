#pragma once

#include <expected>
#include <filesystem>
#include <string>

#include "chip8/chip8.h"
#include "debugger/debugger.h"
#include "media_layer/media_layer.h"
#include "util/stopwatch/stopwatch.h"


class Chip8Emulator {
public:

    /// Load a ROM file and start it. Returns an error message on failure.
    [[nodiscard]]
    auto load_rom(const std::filesystem::path& file) -> std::expected<void, std::string>;

    /// Run the emulator update loop
    auto run() -> void;

private:
    // The CHIP-8 itself
    c8::Chip8 chip;

    // Run control and breakpoints for the CHIP-8
    c8::Debugger debugger{chip};

    // The media layer, which handles rendering and I/O.
    MediaLayer media_layer;

    // Measures the time between host frames
    Stopwatch<> timer;
};
