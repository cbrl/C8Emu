#pragma once

#include <chrono>
#include <cstdint>
#include <expected>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <utility>

#include "chip8/chip8.h"

namespace c8 {

/// Why execution stopped
enum class StopReason : uint8_t {
    none,
    user,         ///< The user paused execution
    step,         ///< A step completed
    breakpoint,   ///< The PC reached a breakpoint
    fault,        ///< An instruction faulted. StopEvent::fault has the details.
    left_program, ///< The PC left the loaded program
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


/// The work that one Debugger::run_frames() call did
struct RunStats {
    uint32_t frames           = 0;     ///< Frames completed
    uint64_t instructions     = 0;     ///< Instructions executed
    bool     deadline_reached = false; ///< The deadline stopped the run before all frames completed
};


/**
 * @class Debugger
 *
 * @brief Run control and breakpoints for a Chip8
 *
 * @details The debugger runs the machine in whole frames. It stops before an
 *          instruction at a breakpoint, on a fault, and (as an option) when the
 *          PC leaves the program. Breakpoints are addresses, so odd addresses
 *          and addresses below the program work.
 *
 *          The debugger keeps a reference to the machine. The machine must
 *          outlive the debugger.
 */
class Debugger {
public:
    using clock = std::chrono::steady_clock;

    /// The debugger runs the machine if it has a program
    explicit Debugger(Chip8& machine) noexcept;

    //--------------------------------------------------------------------------------
    // Program
    //--------------------------------------------------------------------------------

    /// Load a program into the machine and run it. On failure, nothing changes.
    [[nodiscard]]
    auto load_program(std::span<const uint8_t> program, uint16_t origin = MachineState::program_start)
        -> std::expected<void, std::string>;

    /// Reset the machine and run the program again. A breakpoint at the origin stops it at once.
    auto reset() -> void;


    //--------------------------------------------------------------------------------
    // Run control
    //--------------------------------------------------------------------------------

    /// Run the program. The instruction at the PC runs even if it has a breakpoint.
    auto run() noexcept -> void;

    /// Pause execution. If execution already stopped, the stop reason does not change.
    auto pause() noexcept -> void;

    /**
     * @brief Run one instruction and pause
     *
     * @details The instruction runs even if it has a breakpoint. If a draw
     *          left the rest of the frame idle, the frame ends first. The
     *          timers change only if the step ends a frame.
     */
    auto step_into() noexcept -> void;

    /**
     * @brief Run frames until a stop or a deadline
     *
     * @details The call does nothing while execution is paused. The deadline
     *          is checked at each frame end and every few thousand
     *          instructions, so at least one frame completes or stops.
     *
     * @param[in] frames    The number of frames to run
     * @param[in] deadline  The time at which to return, even if frames remain
     */
    auto run_frames(uint32_t frames, clock::time_point deadline) noexcept -> RunStats;

    [[nodiscard]]
    auto is_running() const noexcept -> bool {
        return running;
    }

    /// Why execution stopped. StopReason::none while running.
    [[nodiscard]]
    auto last_stop() const noexcept -> const StopEvent& {
        return stop_event;
    }

    /// The newest stop event, once. A frontend uses it to log a stop or to show its location.
    [[nodiscard]]
    auto take_stop_event() noexcept -> std::optional<StopEvent>;


    //--------------------------------------------------------------------------------
    // Options
    //--------------------------------------------------------------------------------

    /// True if execution stops when the PC leaves the loaded program
    [[nodiscard]]
    auto break_on_leave_program() const noexcept -> bool {
        return leave_program_stops;
    }

    auto set_break_on_leave_program(bool enable) noexcept -> void {
        leave_program_stops = enable;
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

    /// Replace all breakpoints, for example after the source moved them
    auto replace_breakpoints(std::set<uint16_t> addresses) noexcept -> void {
        breakpoint_set = std::move(addresses);
    }

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

private:
    // Clear the stop state after a load or a reset, and run if a program exists
    auto start() noexcept -> void;

    // The reason to stop before the next cycle, if one exists
    [[nodiscard]]
    auto check_stop(bool use_breakpoints) const noexcept -> std::optional<StopEvent>;

    auto stop(const StopEvent& event) noexcept -> void;

    Chip8& chip;

    std::set<uint16_t> breakpoint_set;
    bool leave_program_stops = true;

    bool running = false;
    bool skip_breakpoint = false;
    StopEvent stop_event{};
    std::optional<StopEvent> unread_stop;
};

} // namespace c8
