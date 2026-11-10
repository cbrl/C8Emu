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
#include "chip8/variant.h"

namespace c8 {

/// The result of one Chip8::cycle() call
struct CycleResult {
    bool      executed  = false; ///< An instruction ran. False for an idle cycle, an exit, and a fault.
    bool      frame_end = false; ///< The frame ended after this cycle, and the timers decremented
    bool      exited    = false; ///< 00FD stopped the program. The state did not change.
    FaultInfo fault;             ///< The fault if fault.kind is not Fault::none. The state did not change.
};


/**
 * @class Chip8
 *
 * @brief A CHIP-8 machine and the program it runs
 *
 * @details The variant selects the instruction set, the memory size, and the
 *          display modes (CHIP-8, Hires CHIP-8, SUPER-CHIP, or XO-CHIP).
 *
 *          The machine does not read the clock. Time advances only through
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
        Variant variant = presets[0].variant,
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
     *          A program that differs from the last one clears the flag registers.
     *
     * @param[in] program  The program bytes
     * @param[in] origin   The address of the first byte
     * @param[in] variant  The new variant, or std::nullopt to keep the current one
     *
     * @return An error message on failure
     */
    [[nodiscard]]
    auto load_program(
        std::span<const uint8_t> program,
        uint16_t origin = MachineState::program_start,
        std::optional<Variant> variant = std::nullopt
    ) -> std::expected<void, std::string>;

    /// Set the power-on state and load the last program again. Held keys and the flag registers stay.
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

    /// The largest program that fits in the memory of a variant at an origin
    [[nodiscard]]
    static constexpr auto max_program_size(Variant variant, uint16_t at = MachineState::program_start) noexcept -> size_t {
        const size_t size = memory_size(variant);
        return (at < size) ? (size - at) : 0;
    }

    /// The largest program that fits in memory at an origin
    [[nodiscard]]
    auto max_program_size(uint16_t at = MachineState::program_start) const noexcept -> size_t {
        return max_program_size(machine_variant, at);
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
     *          If the instruction faults or exits (00FD), the state does not
     *          change and the cycle does not count.
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

    /// The instruction set and the hardware
    [[nodiscard]]
    auto variant() const noexcept -> Variant {
        return machine_variant;
    }

    /**
     * @brief Change the variant and reset the machine
     *
     * @details The memory size and the display change, so the machine resets
     *          and loads the program again. If the program does not fit in the
     *          memory of the variant, nothing changes. The same variant does nothing.
     *
     * @return An error message on failure
     */
    [[nodiscard]]
    auto set_variant(Variant variant) -> std::expected<void, std::string>;

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
     * @brief Use the variant, the quirks, and the instructions per frame of a preset
     *
     * @details If the variant changes, the machine resets (see set_variant()).
     *          If not, the state does not change. On failure, nothing changes.
     *
     * @return An error message on failure
     */
    [[nodiscard]]
    auto apply_preset(const Preset& preset) -> std::expected<void, std::string>;

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
    Variant machine_variant;
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
