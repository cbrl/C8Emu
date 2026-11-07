#pragma once

#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "chip8/machine_state.h"
#include "chip8/quirks.h"

namespace c8 {

/// Why execution stopped
enum class StopReason : uint8_t {
    none,
    user,         ///< The user paused execution
    breakpoint,   ///< The PC reached a breakpoint
    fault,        ///< An instruction faulted. StopEvent::fault has the details.
    left_program, ///< The PC left the loaded program
    key_wait,     ///< FX0A waits for a key press
};

/// A stop of execution and where it happened
struct StopEvent {
    StopReason reason = StopReason::none;
    uint16_t   pc     = 0;
    FaultInfo  fault;

    friend constexpr auto operator==(const StopEvent&, const StopEvent&) -> bool = default;
};

/// Text for the user, for example "Fault: stack underflow at 0x0204 (00EE)". Empty for StopReason::none.
[[nodiscard]]
auto describe(const StopEvent& event) -> std::string;


/**
 * @class Chip8
 *
 * @brief A CHIP-8 machine with run control and breakpoints
 *
 * @details The machine does not read the clock. The caller runs cycles at the
 *          clock rate and calls tick_timers() at 60 Hz. The same calls always
 *          give the same result, which makes tests and stepping deterministic.
 */
class Chip8 {
public:
    static constexpr uint32_t min_clock_rate = 1;
    static constexpr uint32_t max_clock_rate = 1'000'000;

    explicit Chip8(const Quirks& quirks = presets[0].quirks);

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

    /// Set the power-on state and load the last program again. The machine runs if a program exists.
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

    /// Pause execution. If execution already stopped for another reason, the reason does not change.
    auto pause() noexcept -> void;

    /// Resume execution. The instruction at the PC runs even if it has a breakpoint.
    auto resume() noexcept -> void;

    /// Run the instruction at the PC, even if it has a breakpoint. The pause state does not change.
    auto step() noexcept -> void;

    /// Run one instruction if the machine is not paused
    auto run_cycle() noexcept -> void;

    /// Decrement the delay and sound timers once. Call it at 60 Hz.
    auto tick_timers() noexcept -> void;

    [[nodiscard]]
    auto is_paused() const noexcept -> bool {
        return paused;
    }

    /// The last reason execution stopped. StopReason::none while running.
    [[nodiscard]]
    auto last_stop() const noexcept -> const StopEvent& {
        return stop_event;
    }

    /// True while FX0A waits for a key
    [[nodiscard]]
    auto waiting_for_key() const noexcept -> bool {
        return key_wait_register.has_value();
    }

    /// True while the sound timer is non-zero
    [[nodiscard]]
    auto sound_active() const noexcept -> bool {
        return machine.sound_timer > 0;
    }


    //--------------------------------------------------------------------------------
    // Input
    //--------------------------------------------------------------------------------

    /// Set the state of a key (0x0-0xF). A press completes a pending FX0A.
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

    /// The instruction rate in Hz
    [[nodiscard]]
    auto get_clock_rate() const noexcept -> uint32_t {
        return clock_rate;
    }

    /// Set the instruction rate in Hz. The value is clamped to [min_clock_rate, max_clock_rate].
    auto set_clock_rate(uint32_t rate) noexcept -> void;

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
    // Breakpoints
    //--------------------------------------------------------------------------------

    auto add_breakpoint(uint16_t address) -> void {
        breakpoint_set.insert(address);
    }

    auto remove_breakpoint(uint16_t address) -> void {
        breakpoint_set.erase(address);
    }

    /// Add or remove a breakpoint. Returns true if the breakpoint now exists.
    auto toggle_breakpoint(uint16_t address) -> bool;

    auto clear_breakpoints() noexcept -> void {
        breakpoint_set.clear();
    }

    [[nodiscard]]
    auto has_breakpoint(uint16_t address) const -> bool {
        return breakpoint_set.contains(address);
    }

    [[nodiscard]]
    auto breakpoints() const noexcept -> const std::set<uint16_t>& {
        return breakpoint_set;
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
    auto execute_cycle() noexcept -> void;
    auto stop(const StopEvent& event) noexcept -> void;
    auto next_seed() noexcept -> uint32_t;

    MachineState machine;
    Quirks quirk_set;

    // The last loaded program and its address
    std::vector<uint8_t> image;
    uint16_t origin = MachineState::program_start;

    // Run control
    bool paused = true;
    bool skip_breakpoint = false;
    StopEvent stop_event{};
    std::set<uint16_t> breakpoint_set;

    // The register that receives the key while FX0A waits
    std::optional<uint8_t> key_wait_register;

    // The instruction rate in Hz
    uint32_t clock_rate = 500;

    // Random number seeds
    std::optional<uint32_t> fixed_seed;
    uint64_t seed_counter = 0;
};

} // namespace c8
