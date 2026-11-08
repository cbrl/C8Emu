#pragma once

#include <atomic>
#include <cstdint>
#include <span>

namespace c8 {

/**
 * @class ToneGenerator
 *
 * @brief Makes the CHIP-8 beep as a square wave of 32-bit float samples
 *
 * @details The setters are safe to call from any thread. Only one thread (the
 *          audio thread) may call generate(). The gain ramps over 5 ms when the
 *          tone starts, stops, or changes volume, so the tone does not click.
 */
class ToneGenerator {
public:
    static constexpr float peak_amplitude    = 0.25f;  ///< The peak of a sample at volume 1.0
    static constexpr float ramp_seconds      = 0.005f; ///< The time for the gain to go from 0 to 1
    static constexpr float default_frequency = 440.0f;
    static constexpr float min_frequency     = 20.0f;

    explicit ToneGenerator(uint32_t sample_rate) noexcept;

    /// Start or stop the tone
    auto set_active(bool on) noexcept -> void {
        active.store(on, std::memory_order_relaxed);
    }

    [[nodiscard]]
    auto is_active() const noexcept -> bool {
        return active.load(std::memory_order_relaxed);
    }

    /// Set the volume. The value is clamped to [0, 1].
    auto set_volume(float volume) noexcept -> void;

    [[nodiscard]]
    auto volume() const noexcept -> float {
        return target_volume.load(std::memory_order_relaxed);
    }

    /// Set the pitch in Hz. The value is clamped to [min_frequency, sample_rate / 2].
    auto set_frequency(float hz) noexcept -> void;

    [[nodiscard]]
    auto frequency() const noexcept -> float {
        return pitch.load(std::memory_order_relaxed);
    }

    [[nodiscard]]
    auto sample_rate() const noexcept -> uint32_t {
        return rate;
    }

    /// Fill the buffer with the next samples
    auto generate(std::span<float> samples) noexcept -> void;

    /// The position in the current wave period, in [0, 1). Read it on the thread that calls generate().
    [[nodiscard]]
    auto phase() const noexcept -> double {
        return phase_position;
    }

    /// The current gain, in [0, 1]. Read it on the thread that calls generate().
    [[nodiscard]]
    auto gain() const noexcept -> float {
        return current_gain;
    }

private:
    uint32_t rate;

    std::atomic<bool>  active{false};
    std::atomic<float> target_volume{1.0f};
    std::atomic<float> pitch{default_frequency};

    // Only the thread that calls generate() uses these
    double phase_position = 0.0;
    float  current_gain   = 0.0f;
};

} // namespace c8
