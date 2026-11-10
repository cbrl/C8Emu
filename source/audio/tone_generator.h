#pragma once

#include <array>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <span>

namespace c8 {

/**
 * @class ToneGenerator
 *
 * @brief Makes the CHIP-8 beep as 32-bit float samples
 *
 * @details The beep is a square wave, or an XO-CHIP audio pattern: 128 one-bit
 *          samples that repeat at a playback rate.
 *
 *          The setters are safe to call from any thread. Only one thread (the
 *          audio thread) may call generate(). The gain ramps over 5 ms when the
 *          tone starts, stops, or changes volume, so the tone does not click.
 */
class ToneGenerator {
public:
    static constexpr float peak_amplitude    = 0.25f;  ///< The peak of a sample at volume 1.0
    static constexpr float ramp_seconds      = 0.005f; ///< The time for the gain to go from 0 to 1
    static constexpr float default_frequency = 440.0f;
    static constexpr float min_frequency     = 20.0f;
    static constexpr size_t pattern_bits     = 128;

    /// The XO-CHIP playback rate of a pitch register value: 4000 * 2^((pitch - 64) / 48) bits per second
    [[nodiscard]]
    static auto pitch_to_rate(uint8_t pitch) noexcept -> float {
        return 4000.0f * std::exp2((static_cast<float>(pitch) - 64.0f) / 48.0f);
    }

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

    /**
     * @brief Play an XO-CHIP audio pattern instead of the square wave
     *
     * @details Bit 7 of byte 0 plays first. The rate is clamped to
     *          [1, sample_rate()]. The four values are separate atomics, so a
     *          caller on another thread must keep generate() from running
     *          during the call (Beeper locks its audio stream).
     */
    auto set_pattern(std::span<const uint8_t, 16> pattern, float bits_per_second) noexcept -> void;

    /// Play the square wave again
    auto clear_pattern() noexcept -> void {
        pattern_mode.store(false, std::memory_order_relaxed);
    }

    [[nodiscard]]
    auto has_pattern() const noexcept -> bool {
        return pattern_mode.load(std::memory_order_relaxed);
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
    auto generate_pattern(std::span<float> samples, float target, float ramp_step) noexcept -> void;

    uint32_t rate;

    std::atomic<bool>  active{false};
    std::atomic<float> target_volume{1.0f};
    std::atomic<float> pitch{default_frequency};

    // The XO-CHIP pattern as two big-endian halves, and its playback rate
    std::atomic<bool>     pattern_mode{false};
    std::atomic<uint64_t> pattern_high{0};
    std::atomic<uint64_t> pattern_low{0};
    std::atomic<float>    pattern_rate{4000.0f};

    // Only the thread that calls generate() uses these
    double phase_position = 0.0; ///< [0, 1) for the square wave, [0, 128) for a pattern
    float  current_gain   = 0.0f;
};

} // namespace c8
