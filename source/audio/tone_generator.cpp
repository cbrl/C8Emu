#include "tone_generator.h"

#include <algorithm>
#include <cmath>

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


auto ToneGenerator::set_pattern(std::span<const uint8_t, 16> pattern, float bits_per_second) noexcept -> void {
    uint64_t high = 0;
    uint64_t low = 0;
    for (size_t b = 0; b < 8; ++b) {
        high = (high << 8) | pattern[b];
        low  = (low << 8) | pattern[b + 8];
    }

    pattern_high.store(high, std::memory_order_relaxed);
    pattern_low.store(low, std::memory_order_relaxed);
    // A NaN fails the comparison, so it becomes 1. More than one bit per sample only aliases.
    const float max_rate = static_cast<float>(rate);
    const float value = (bits_per_second > 1.0f) ? std::min(bits_per_second, max_rate) : 1.0f;
    pattern_rate.store(value, std::memory_order_relaxed);
    pattern_mode.store(true, std::memory_order_relaxed);
}


auto ToneGenerator::generate(std::span<float> samples) noexcept -> void {
    const float target = is_active() ? volume() : 0.0f;
    const float ramp_step = 1.0f / (ramp_seconds * static_cast<float>(rate));

    if (has_pattern()) {
        generate_pattern(samples, target, ramp_step);
        return;
    }

    // The square wave uses a phase in [0, 1). A pattern used [0, 128).
    if (phase_position >= 1.0) {
        phase_position = 0.0;
    }

    const double step = static_cast<double>(frequency()) / static_cast<double>(rate);

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


auto ToneGenerator::generate_pattern(std::span<float> samples, float target, float ramp_step) noexcept -> void {
    const uint64_t high = pattern_high.load(std::memory_order_relaxed);
    const uint64_t low  = pattern_low.load(std::memory_order_relaxed);
    const double step = static_cast<double>(pattern_rate.load(std::memory_order_relaxed)) / static_cast<double>(rate);
    constexpr auto bits = static_cast<double>(pattern_bits);

    for (auto& sample : samples) {
        if (current_gain < target) {
            current_gain = std::min(current_gain + ramp_step, target);
        }
        else if (current_gain > target) {
            current_gain = std::max(current_gain - ramp_step, target);
        }

        if (current_gain == 0.0f) {
            phase_position = 0.0;
            sample = 0.0f;
            continue;
        }

        const auto index = static_cast<uint32_t>(phase_position) % pattern_bits;
        const uint64_t half = (index < 64) ? high : low;
        const bool bit = ((half >> (63 - (index % 64))) & 1) != 0;

        sample = (bit ? 1.0f : -1.0f) * current_gain * peak_amplitude;

        phase_position = std::fmod(phase_position + step, bits);
    }
}

} // namespace c8
