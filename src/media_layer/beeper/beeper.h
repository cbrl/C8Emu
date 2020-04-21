#pragma once

#include <SDL2/SDL.h>

class Beeper {
public:
    ~Beeper();

    bool init_audio();
    void deinit_audio();

    void start_beep();
    void stop_beep();

    [[nodiscard]]
    uint32_t get_amplitude() const noexcept {
        return amplitude;
    }
    void set_amplitude(uint32_t value) noexcept {
        amplitude = value;
    }

    [[nodiscard]]
    float get_frequency() const noexcept {
        return frequency;
    }
    void set_frequency(float value) noexcept {
        frequency = value;
    }

private:

    static void audio_callback(void* user_data, Uint8* raw_buffer, int bytes);

    SDL_AudioDeviceID audio_device = 0;

    static constexpr uint32_t sample_rate = 44100;

    uint32_t amplitude = 280000;
    float frequency = 441.0f;
    uint32_t sample_num = 0;
};