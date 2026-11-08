#include "tone_generator.h"

#include <algorithm>

namespace c8 {

ToneGenerator::ToneGenerator(uint32_t sample_rate) noexcept : rate(std::max<uint32_t>(sample_rate, 1)) {
    set_frequency(default_frequency);
}


auto ToneGenerator::set_volume(float volume) noexcept -> void {
    // A NaN fails both comparisons, so it becomes 0
    const float value = (volume > 0.0f) ? std::min(volume, 1.0f) : 0.0f;
    target_volume.store(value, std::memory_order_relaxed);
}


auto ToneGenerator::set_frequency(float hz) noexcept -> void {
    // Below half the sample rate, one sample moves the phase by less than half a period
    const float max_frequency = static_cast<float>(rate) / 2.0f;
    const float value = (hz > min_frequency) ? std::min(hz, max_frequency) : min_frequency;
    pitch.store(std::min(value, max_frequency), std::memory_order_relaxed);
}


auto ToneGenerator::generate(std::span<float> samples) noexcept -> void {
    const float target = is_active() ? volume() : 0.0f;
    const double step = static_cast<double>(frequency()) / static_cast<double>(rate);
    const float ramp_step = 1.0f / (ramp_seconds * static_cast<float>(rate));

    for (auto& sample : samples) {
        if (current_gain < target) {
            current_gain = std::min(current_gain + ramp_step, target);
        }
        else if (current_gain > target) {
            current_gain = std::max(current_gain - ramp_step, target);
        }

        // In silence, hold the phase at 0, so each beep starts at the same point of the wave
        if (current_gain == 0.0f) {
            phase_position = 0.0;
            sample = 0.0f;
            continue;
        }

        const float wave = (phase_position < 0.5) ? 1.0f : -1.0f;
        sample = wave * current_gain * peak_amplitude;

        // The step is at most 0.5, so one subtraction keeps the phase in [0, 1)
        phase_position += step;
        if (phase_position >= 1.0) {
            phase_position -= 1.0;
        }
    }
}

} // namespace c8
