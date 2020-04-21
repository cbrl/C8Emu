#include "beeper.h"
#include <iostream>


Beeper::~Beeper() {
    deinit_audio();
}

bool Beeper::init_audio() {
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

void Beeper::audio_callback(void* user_data, Uint8* raw_buffer, int bytes) {
    auto& beeper = *static_cast<Beeper*>(user_data);

    float* buffer    = reinterpret_cast<float*>(raw_buffer);
    const int length = bytes / 4; //4 bytes per sample for AUDIO_F32SYS

    for(int i = 0; i < length; ++i, ++beeper.sample_num) {
        const float time = static_cast<float>(beeper.sample_num) / static_cast<float>(beeper.sample_rate);
        buffer[i] = static_cast<float>(beeper.amplitude * sin(2.0f * M_PI * beeper.frequency * time)); //render sine wave
    }
}

void Beeper::deinit_audio() {
    if (audio_device != 0) {
        SDL_CloseAudioDevice(audio_device);
    }
}

void Beeper::start_beep() {
    SDL_PauseAudioDevice(audio_device, 0);
}

void Beeper::stop_beep() {
    SDL_PauseAudioDevice(audio_device, 1);
}