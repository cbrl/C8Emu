#pragma once

#include <algorithm>
#include <cstdint>

namespace c8 {

/// The frontend settings
struct Settings {
    static constexpr uint32_t min_display_scale = 1;
    static constexpr uint32_t max_display_scale = 32;

    // The range of the host frame limit. The emulated frame rate is fixed at c8::Chip8::frame_rate (60).
    // The main loop catches up at most 4 emulated frames, so a host rate below 15 FPS slows the emulation.
    // The minimum of 30 keeps a margin above that.
    static constexpr uint32_t min_frame_limit = 30;
    static constexpr uint32_t max_frame_limit = 1000;

    uint32_t display_scale    = 10;         ///< The size of a CHIP-8 pixel in screen pixels
    uint32_t background_color = 0x000000FF; ///< RGBA
    uint32_t foreground_color = 0xFFFFFFFF; ///< RGBA

    bool     frame_limit_enabled = true;
    uint32_t frame_limit         = 60;      ///< Frames per second

    float volume = 0.5f;                    ///< [0, 1]
    bool  muted  = false;

    /// Clamp each value to its range
    auto clamp() noexcept -> void {
        display_scale = std::clamp(display_scale, min_display_scale, max_display_scale);
        frame_limit   = std::clamp(frame_limit, min_frame_limit, max_frame_limit);
        volume        = (volume > 0.0f) ? std::min(volume, 1.0f) : 0.0f;
    }
};

} // namespace c8
