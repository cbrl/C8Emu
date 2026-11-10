#pragma once

#include <cstdint>
#include <span>

#include <SDL3/SDL.h>

#include "audio/tone_generator.h"

namespace c8 {

/**
 * @class Beeper
 *
 * @brief Plays the CHIP-8 beep on the default audio device
 *
 * @details The device plays all the time. The tone generator gives silence
 *          while the beep is off, so the beep can fade in and out. If no
 *          device opens, the beeper logs a warning and stays silent.
 *          The audio subsystem of SDL must be ready before construction.
 */
class Beeper {
public:
    static constexpr uint32_t sample_rate = 44100;

    Beeper();
    ~Beeper();

    // The audio thread keeps a pointer to the beeper, so the beeper cannot move
    Beeper(const Beeper&) = delete;
    Beeper(Beeper&&) = delete;
    auto operator=(const Beeper&) -> Beeper& = delete;
    auto operator=(Beeper&&) -> Beeper& = delete;

    /// Start or stop the beep
    auto set_active(bool on) noexcept -> void {
        tone.set_active(on);
    }

    /// Set the volume in [0, 1]
    auto set_volume(float volume) noexcept -> void {
        tone.set_volume(volume);
    }

    /// Play an XO-CHIP audio pattern at a playback rate (bits per second)
    auto set_pattern(std::span<const uint8_t, 16> pattern, float bits_per_second) noexcept -> void;

    /// Play the plain square wave
    auto clear_pattern() noexcept -> void {
        tone.clear_pattern();
    }

    /// True if an audio device is open
    [[nodiscard]]
    auto available() const noexcept -> bool {
        return stream != nullptr;
    }

private:
    static auto SDLCALL feed(void* user_data, SDL_AudioStream* audio_stream, int additional_bytes, int total_bytes) -> void;

    ToneGenerator tone{sample_rate};
    SDL_AudioStream* stream = nullptr;
};

} // namespace c8
