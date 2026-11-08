#pragma once

#include <chrono>
#include <cstdint>

#include "chip8/machine_state.h"

namespace c8 {

/// The measured emulation speed
struct EmulationRate {
    double instructions_per_second = 0.0;
    double frames_per_second       = 0.0;
};


/// Measures the emulation speed from the machine counters, once each second
class RateMeter {
public:
    using clock = std::chrono::steady_clock;

    auto update(const MachineState& state, clock::time_point now) noexcept -> void {
        // A reset sets the counters back to zero. Start a new measurement after that happens.
        const bool counters_reset = (state.total_cycles < start_cycles) || (state.frame_count < start_frames);
        const auto elapsed = std::chrono::duration<double>(now - start_time);

        if (!counters_reset && started && (elapsed < std::chrono::seconds{1})) {
            return;
        }

        if (!counters_reset && started && (elapsed.count() > 0.0)) {
            current.instructions_per_second = static_cast<double>(state.total_cycles - start_cycles) / elapsed.count();
            current.frames_per_second       = static_cast<double>(state.frame_count - start_frames) / elapsed.count();
        }

        started      = true;
        start_time   = now;
        start_cycles = state.total_cycles;
        start_frames = state.frame_count;
    }

    [[nodiscard]]
    auto rate() const noexcept -> const EmulationRate& {
        return current;
    }

private:
    bool              started = false;
    clock::time_point start_time{};
    uint64_t          start_cycles = 0;
    uint64_t          start_frames = 0;
    EmulationRate     current;
};

} // namespace c8
