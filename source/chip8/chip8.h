#pragma once

#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "chip8/machine_state.h"
#include "chip8/quirks.h"

namespace c8 {

/// The result of one Chip8::cycle() call
struct CycleResult {
    bool      executed  = false; ///< An instruction ran. False for an idle cycle and for a fault.
    bool      frame_end = false; ///< The frame ended after this cycle, and the timers decremented
    FaultInfo fault;             ///< The fault if fault.kind is not Fault::none. The state did not change.
};


/**
 * @class Chip8
 *
 * @brief A CHIP-8 machine and the program it runs
 *
 * @details The machine does not read the clock. Time advances only through
 *          cycle(): each frame is instructions_per_frame() cycles, and the
 *          timers decrement at the end of a frame. The same calls always give
 *          the same state, so stepping and running agree.
 *
 *          Run control (pause, breakpoints, stepping) belongs to c8::Debugger.
 */
class Chip8 {
public:
    /// Emulated frames per second. The delay and sound timers count at this rate.
    static constexpr uint32_t frame_rate = 60;

    static constexpr uint32_t min_instructions_per_frame = 1;
    static constexpr uint32_t max_instructions_per_frame = 100'000;

    explicit Chip8(
        const Quirks& quirks = presets[0].quirks,
        uint32_t instructions_per_frame = presets[0].instructions_per_frame
    );

    //--------------------------------------------------------------------------------
    // Program
    //--------------------------------------------------------------------------------

    /**
     * @brief Load a program and reset the machine
     *
     * @details The machine keeps a copy of the program, so reset() can load it
     *          again. If the program is empty or too large, nothing changes.
     *
     * @param[in] program  The program bytes
     * @param[in] origin   The address of the first byte
     *
     * @return An error message on failure
     */
    [[nodiscard]]
    auto load_program(std::span<const uint8_t> program, uint16_t origin = MachineState::program_start)
        -> std::expected<void, std::string>;

    /// Set the power-on state and load the last program again. Held keys stay held.
    auto reset() -> void;

    /// The bytes of the last loaded program
    [[nodiscard]]
    auto program() const noexcept -> std::span<const uint8_t> {
        return image;
    }

    /// The address of the first program byte
    [[nodiscard]]
    auto program_origin() const noexcept -> uint16_t {
        return origin;
    }

    /// The address after the last program byte
    [[nodiscard]]
    auto program_end() const noexcept -> uint32_t {
        return static_cast<uint32_t>(origin + image.size());
    }

    /// The largest program that fits in memory at an origin
    [[nodiscard]]
    auto max_program_size(uint16_t at = MachineState::program_start) const noexcept -> size_t {
        return (at < machine.memory.size()) ? (machine.memory.size() - at) : 0;
    }


    //--------------------------------------------------------------------------------
    // Execution
    //--------------------------------------------------------------------------------

    /**
     * @brief Run one cycle
     *
     * @details A cycle runs the instruction at the PC. After a draw with the
     *          display_wait quirk, the next cycle is idle: it only ends the
     *          frame. The frame also ends after instructions_per_frame() cycles.
     *          At the end of a frame, the timers decrement.
     *
     *          If the instruction faults, the state does not change.
     */
    auto cycle() noexcept -> CycleResult;

    /// True while FX0A waits for a key
    [[nodiscard]]
    auto waiting_for_key() const noexcept -> bool {
        return machine.in_key_wait();
    }

    /// True while the sound timer is non-zero
    [[nodiscard]]
    auto sound_active() const noexcept -> bool {
        return machine.sound_timer > 0;
    }


    //--------------------------------------------------------------------------------
    // Input
    //--------------------------------------------------------------------------------

    /// Set the state of a key (0x0-0xF). A waiting FX0A sees the change in its next cycle.
    auto set_key(uint8_t key, bool down) noexcept -> void;


    //--------------------------------------------------------------------------------
    // Configuration
    //--------------------------------------------------------------------------------

    [[nodiscard]]
    auto quirks() const noexcept -> const Quirks& {
        return quirk_set;
    }

    auto set_quirks(const Quirks& quirks) noexcept -> void {
        quirk_set = quirks;
    }

    /// The number of cycles in each 60 Hz frame
    [[nodiscard]]
    auto instructions_per_frame() const noexcept -> uint32_t {
        return ipf;
    }

    /// Set the cycles in each frame. The value is clamped to [min_instructions_per_frame, max_instructions_per_frame].
    auto set_instructions_per_frame(uint32_t count) noexcept -> void;

    /**
     * @brief Set the seed of the CXNN random number generator
     *
     * @details With a seed, each reset starts the same random sequence. Without
     *          a seed (nullopt), each reset uses a new seed. The change applies now.
     */
    auto set_seed(std::optional<uint32_t> seed) noexcept -> void;

    [[nodiscard]]
    auto get_seed() const noexcept -> std::optional<uint32_t> {
        return fixed_seed;
    }


    //--------------------------------------------------------------------------------
    // State
    //--------------------------------------------------------------------------------

    /// The machine state, for display
    [[nodiscard]]
    auto state() const noexcept -> const MachineState& {
        return machine;
    }

    /// The machine state, for edits by a debugger or a test
    [[nodiscard]]
    auto state() noexcept -> MachineState& {
        return machine;
    }

private:
    auto end_frame() noexcept -> void;
    auto next_seed() noexcept -> uint32_t;

    MachineState machine;
    Quirks quirk_set;
    uint32_t ipf;

    // The last loaded program and its address
    std::vector<uint8_t> image;
    uint16_t origin = MachineState::program_start;

    // Random number seeds
    std::optional<uint32_t> fixed_seed;
    uint64_t seed_counter = 0;
};

} // namespace c8
