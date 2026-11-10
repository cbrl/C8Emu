#include "beeper.h"

#include "util/log.h"

#include <algorithm>
#include <array>
#include <span>

namespace c8 {

Beeper::Beeper() {
    if (SDL_WasInit(SDL_INIT_AUDIO) == 0) {
        return;
    }

    const auto spec = SDL_AudioSpec{
        .format   = SDL_AUDIO_F32,
        .channels = 1,
        .freq     = static_cast<int>(sample_rate),
    };

    stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, Beeper::feed, this);
    if (stream == nullptr) {
        log_warning("Cannot open the audio device: {}", SDL_GetError());
        return;
    }

    // The device starts paused. It plays silence until the beep starts.
    if (!SDL_ResumeAudioStreamDevice(stream)) {
        log_warning("Cannot start the audio device: {}", SDL_GetError());
    }
}


Beeper::~Beeper() {
    // This stops the audio thread before the tone generator is destroyed
    if (stream != nullptr) {
        SDL_DestroyAudioStream(stream);
    }
}


auto Beeper::set_pattern(std::span<const uint8_t, 16> pattern, float bits_per_second) noexcept -> void {
    // SDL holds the stream lock while it calls feed(), so the audio thread never sees half a pattern
    if (stream != nullptr) {
        SDL_LockAudioStream(stream);
    }

    tone.set_pattern(pattern, bits_per_second);

    if (stream != nullptr) {
        SDL_UnlockAudioStream(stream);
    }
}


auto SDLCALL Beeper::feed(void* user_data, SDL_AudioStream* audio_stream, int additional_bytes, [[maybe_unused]] int total_bytes) -> void {
    auto& beeper = *static_cast<Beeper*>(user_data);

    if (additional_bytes <= 0) {
        return;
    }

    auto buffer = std::array<float, 512>{};
    auto remaining = static_cast<size_t>(additional_bytes) / sizeof(float);

    while (remaining > 0) {
        const auto samples = std::span{buffer}.first(std::min(remaining, buffer.size()));
        beeper.tone.generate(samples);

        SDL_PutAudioStreamData(audio_stream, samples.data(), static_cast<int>(samples.size_bytes()));
        remaining -= samples.size();
    }
}

} // namespace c8
