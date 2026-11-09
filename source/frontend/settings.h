#pragma once

#include <algorithm>
#include <cstdint>

#include "assembler/disassembler.h"

namespace c8 {

/// The frontend settings
struct Settings {
    // The range of the host frame limit. The emulated frame rate is fixed at c8::Chip8::frame_rate (60).
    // The main loop catches up at most 4 emulated frames, so a host rate below 15 FPS slows the emulation.
    // The minimum of 30 keeps a margin above that.
    static constexpr uint32_t min_frame_limit = 30;
    static constexpr uint32_t max_frame_limit = 1000;

    // While the turbo key is held, the emulation runs this many times faster
    static constexpr uint32_t min_turbo_factor = 2;
    static constexpr uint32_t max_turbo_factor = 16;

    uint32_t background_color = 0x000000FF; ///< RGBA
    uint32_t foreground_color = 0xFFFFFFFF; ///< RGBA

    bool     frame_limit_enabled = true;
    uint32_t frame_limit         = 60;      ///< Frames per second

    float volume = 0.5f;                    ///< [0, 1]
    bool  muted  = false;

    uint32_t turbo_factor = 4;              ///< The speed multiplier while the turbo key is held

    /// How the code editor disassembles a ROM
    DisassemblyMode disassembly_mode = DisassemblyMode::flow;

    /// Clamp each value to its range
    auto clamp() noexcept -> void {
        frame_limit   = std::clamp(frame_limit, min_frame_limit, max_frame_limit);
        turbo_factor  = std::clamp(turbo_factor, min_turbo_factor, max_turbo_factor);
        volume        = (volume > 0.0f) ? std::min(volume, 1.0f) : 0.0f;
    }
};

} // namespace c8
