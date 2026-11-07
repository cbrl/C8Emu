#pragma once

#include <SDL3/SDL.h>

class Beeper {
    static constexpr uint32_t sample_rate = 44100;

public:
    ~Beeper();

    auto init_audio() -> bool;
    auto deinit_audio() -> void;

    auto start_beep() -> void;
    auto stop_beep() -> void;

    [[nodiscard]]
    auto get_amplitude() const noexcept -> uint32_t {
        return amplitude;
    }
    auto set_amplitude(uint32_t value) noexcept -> void {
        amplitude = value;
    }

    [[nodiscard]]
    auto get_frequency() const noexcept -> float {
        return frequency;
    }
    auto set_frequency(float value) noexcept -> void {
        frequency = value;
    }

private:

    static auto SDLCALL audio_callback(void* user_data, SDL_AudioStream* stream, int additional_bytes, int total_bytes) -> void;

    SDL_AudioStream* audio_stream = nullptr;

    uint32_t amplitude = 280000;
    float frequency = 441.0f;
    uint32_t sample_num = 0;
};