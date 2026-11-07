#pragma once

#include <expected>
#include <filesystem>
#include <string>

#include "chip8/chip8.h"
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
    /// Renders the user interface
    auto render() -> void;

    // The CHIP-8 itself
    c8::Chip8 chip;

    // The media layer, which handles rendering and I/O.
    MediaLayer media_layer;

    // The timer used to limit the execution rate
    Stopwatch<> timer;
};
