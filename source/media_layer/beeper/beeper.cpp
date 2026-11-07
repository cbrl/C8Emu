#include "beeper.h"

#include <algorithm>
#include <array>
#include <iostream>
#include <numbers>


Beeper::~Beeper() {
    deinit_audio();
}

auto Beeper::init_audio() -> bool {
    deinit_audio();

    SDL_AudioSpec audio_spec = {};
    audio_spec.format   = SDL_AUDIO_F32;
    audio_spec.channels = 1;
    audio_spec.freq     = sample_rate;

    // The device of the stream starts paused
    audio_stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &audio_spec, Beeper::audio_callback, this);
    if (audio_stream == nullptr) {
        std::cout << "Failed to open SDL audio device " << SDL_GetError() << '\n';
        return false;
    }

    return true;
}

auto SDLCALL Beeper::audio_callback(void* user_data, SDL_AudioStream* stream, int additional_bytes, [[maybe_unused]] int total_bytes) -> void {
    auto& beeper = *static_cast<Beeper*>(user_data);

    if (additional_bytes <= 0) {
        return;
    }

    auto buffer = std::array<float, 512>{};
    auto remaining = static_cast<size_t>(additional_bytes) / sizeof(float);

    while (remaining > 0) {
        const auto length = std::min(remaining, buffer.size());

        for(size_t i = 0; i < length; ++i, ++beeper.sample_num) {
            const auto time = static_cast<float>(beeper.sample_num) / static_cast<float>(beeper.sample_rate);
            buffer[i] = static_cast<float>(beeper.amplitude * sin(2.0f * std::numbers::pi * beeper.frequency * time)); //render sine wave
        }

        SDL_PutAudioStreamData(stream, buffer.data(), static_cast<int>(length * sizeof(float)));
        remaining -= length;
    }
}

auto Beeper::deinit_audio() -> void {
    // SDL_Quit() destroys all audio streams. Do not destroy the stream again after that.
    if (audio_stream != nullptr && SDL_WasInit(SDL_INIT_AUDIO) != 0) {
        SDL_DestroyAudioStream(audio_stream);
    }
    audio_stream = nullptr;
}

auto Beeper::start_beep() -> void {
    SDL_ResumeAudioStreamDevice(audio_stream);
}

auto Beeper::stop_beep() -> void {
    SDL_PauseAudioStreamDevice(audio_stream);
}
