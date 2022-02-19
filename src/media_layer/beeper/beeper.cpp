#include "beeper.h"

#include <iostream>
#include <numbers>


Beeper::~Beeper() {
    deinit_audio();
}

auto Beeper::init_audio() -> bool {
    deinit_audio();

    SDL_AudioSpec audio_spec = {};
    audio_spec.freq     = sample_rate;
    audio_spec.format   = AUDIO_F32SYS;
    audio_spec.channels = 1;
    audio_spec.samples  = 1024;
    audio_spec.callback = Beeper::audio_callback;
    audio_spec.userdata = this;

    audio_device = SDL_OpenAudioDevice(nullptr, 0, &audio_spec, nullptr, 0);
    if (audio_device == 0) {
        std::cout << "Failed to open SDL audio device " << SDL_GetError() << '\n';
        return false;
    }

    return true;
}

auto Beeper::audio_callback(void* user_data, Uint8* raw_buffer, int bytes) -> void {
    auto& beeper = *static_cast<Beeper*>(user_data);

    auto* buffer = reinterpret_cast<float*>(raw_buffer);
    const auto length = bytes / 4; //4 bytes per sample for AUDIO_F32SYS

    for(int i = 0; i < length; ++i, ++beeper.sample_num) {
        const auto time = static_cast<float>(beeper.sample_num) / static_cast<float>(beeper.sample_rate);
        buffer[i] = static_cast<float>(beeper.amplitude * sin(2.0f * std::numbers::pi * beeper.frequency * time)); //render sine wave
    }
}

auto Beeper::deinit_audio() -> void {
    if (audio_device != 0) {
        SDL_CloseAudioDevice(audio_device);
    }
}

auto Beeper::start_beep() -> void {
    SDL_PauseAudioDevice(audio_device, 0);
}

auto Beeper::stop_beep() -> void {
    SDL_PauseAudioDevice(audio_device, 1);
}
