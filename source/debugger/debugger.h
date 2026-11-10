#pragma once

#include <chrono>
#include <cstdint>
#include <expected>
#include <map>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "chip8/chip8.h"
#include "debugger/debug_expression.h"
#include "debugger/history.h"
#include "debugger/profiler.h"
#include "debugger/trace.h"
#include "debugger/watchpoints.h"

namespace c8 {

/// Why execution stopped
enum class StopReason : uint8_t {
    none,
    user,         ///< The user paused execution
    step,         ///< A step, a run to an address, or a frame advance completed
    breakpoint,   ///< The PC reached a breakpoint
    watchpoint,   ///< The next instruction reads or writes watched memory. StopEvent::address has the address.
    condition,    ///< The break condition became true
    fault,        ///< An instruction faulted. StopEvent::fault has the details.
    left_program, ///< The PC left the loaded program
    exited,       ///< The program ran 00FD (SUPER-CHIP and XO-CHIP)
};

/// A stop of execution and where it happened
struct StopEvent {
    StopReason reason  = StopReason::none;
    uint16_t   pc      = 0;
    FaultInfo  fault;
    uint32_t   address = 0;     ///< The watched address that the instruction accesses
    bool       write   = false; ///< The watched access is a write

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
 * @struct Breakpoint
 *
 * @brief A breakpoint at an instruction address, and its settings
 *
 * @details When the PC reaches the address, the breakpoint checks its
 *          condition. If the condition is true, the hit count goes up. The
 *          first `ignore_count` hits do nothing. After them, a breakpoint
 *          stops, and a logpoint (a breakpoint with a log message) writes its
 *          message and does not stop.
 *
 *          A condition or a message with an error does not stop the program
 *          silently: the breakpoint then stops as a plain breakpoint.
 */
struct Breakpoint {
    bool                       enabled      = true;
    Compiled<DebugExpression>  condition;        ///< Not set: always true
    uint32_t                   hit_count    = 0; ///< The hits since the breakpoint was set or the counts were reset
    uint32_t                   ignore_count = 0; ///< The first hits that do not stop or log
    Compiled<MessageTemplate>  log;              ///< Set: the breakpoint is a logpoint

    /// True if the breakpoint has no condition, no ignore count, and no log message
    [[nodiscard]]
    auto is_plain() const noexcept -> bool {
        return !condition.is_set() && (ignore_count == 0) && !log.is_set();
    }

    /// True if the breakpoint writes a message and does not stop
    [[nodiscard]]
    auto is_logpoint() const noexcept -> bool {
        return log.value.has_value();
    }
};


/**
 * @class Debugger
 *
 * @brief Run control, breakpoints, and inspection tools for a Chip8
 *
 * @details The debugger runs the machine in whole frames. It stops before an
 *          instruction at a breakpoint, before an access to watched memory,
 *          when the break condition is true, on a fault, and (as an option)
 *          when the PC leaves the program. Breakpoints are addresses, so odd
 *          addresses and addresses below the program work.
 *
 *          step_over(), step_out(), run_to(), and frame_advance() do not run at
 *          once. They run the machine with a goal, and run_frames() stops when
 *          the machine reaches the goal. So these steps run at the normal speed,
 *          and the timers count as in real time. A breakpoint or a fault stops a
 *          step before its goal.
 *
 *          The tools (watchpoints, the break condition, the trace, and the
 *          profiler) cost time only while they are on. While all of them are
 *          off, a cycle checks one flag.
 *
 *          The history records the state at each frame end. step_back() and
 *          frame_back() use it to go back in time. A replay from a recorded
 *          state presses the same keys at the same instructions, so the keys
 *          must change through set_key().
 *
 *          The debugger keeps a reference to the machine. The machine must
 *          outlive the debugger.
 */
class Debugger {
public:
    using clock = std::chrono::steady_clock;

    /// A logpoint writes at most this many messages in one frame. A note tells how many more there were.
    static constexpr uint32_t max_log_messages_per_frame = 100;

    /// The debugger runs the machine if it has a program
    explicit Debugger(Chip8& machine) noexcept;

    //--------------------------------------------------------------------------------
    // Program
    //--------------------------------------------------------------------------------

    /// Load a program into the machine and run it. A variant also changes the machine. On failure, nothing changes.
    [[nodiscard]]
    auto load_program(
        std::span<const uint8_t> program,
        uint16_t origin = MachineState::program_start,
        std::optional<Variant> variant = std::nullopt
    ) -> std::expected<void, std::string>;

    /// Reset the machine and run the program again. A breakpoint at the origin stops it at once.
    auto reset() -> void;

    /**
     * @brief Restore a save state (see Chip8::load_state())
     *
     * @details Execution continues if it ran before. If not, the stop event
     *          shows the new PC. The history starts again, and the profiler
     *          counts are cleared. On failure, nothing changes.
     */
    [[nodiscard]]
    auto load_state(SaveState save) -> std::expected<void, std::string>;

    /// Change the variant of the machine. On success, the machine resets and runs the program.
    [[nodiscard]]
    auto set_variant(Variant variant) -> std::expected<void, std::string>;

    /**
     * @brief Use a preset in the machine
     *
     * @details If the preset changes the variant, the machine resets and runs
     *          the program. If not, execution continues. On failure, nothing changes.
     */
    [[nodiscard]]
    auto apply_preset(const Preset& preset) -> std::expected<void, std::string>;

    /// True if the machine has a program. Without a program, the run controls do nothing.
    [[nodiscard]]
    auto has_program() const noexcept -> bool {
        return !chip.program().empty();
    }

    /**
     * @brief Set the labels and constants of the loaded program
     *
     * @details All conditions and log messages compile again with the new
     *          names, so they do not keep the values of an old program. A text
     *          that does not compile now keeps its error, and it compiles
     *          again with the next names.
     */
    auto set_symbols(SymbolTable table) -> void;

    [[nodiscard]]
    auto symbols() const noexcept -> const SymbolTable& {
        return symbol_table;
    }

    /// A count that changes with each set_symbols(). A view of other expressions compiles them again when it changes.
    [[nodiscard]]
    auto symbols_version() const noexcept -> uint64_t {
        return symbol_version;
    }


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
    auto step_into() -> void;

    /**
     * @brief Run until the instruction after the one at the PC
     *
     * @details For 2NNN (call) and FX0A (key wait), the machine runs until the
     *          PC reaches the next instruction at the same stack depth, or
     *          until the stack is less deep than now. For other instructions,
     *          this is step_into().
     */
    auto step_over() -> void;

    /// Run until the current subroutine returns. At stack depth 0, nothing happens.
    auto step_out() noexcept -> void;

    /// Run until the PC reaches an address. If the PC is at the address now, the instruction runs first.
    auto run_to(uint16_t address) noexcept -> void;

    /// Run to the end of the current frame, then pause
    auto frame_advance() noexcept -> void;

    /**
     * @brief Go back one instruction and pause
     *
     * @details The machine goes back to a recorded state before the previous
     *          instruction. Then it runs again until the previous instruction
     *          completes, so the state is the state that a step gives.
     *          Breakpoints, the trace, and the profiler do not see the replay.
     *
     * @return False if the history has no earlier state
     */
    auto step_back() -> bool;

    /**
     * @brief Go back to the previous recorded frame end and pause
     *
     * @details A rewind calls this once for each emulated frame.
     *
     * @return False if the history has no earlier state
     */
    auto frame_back() -> bool;

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
    auto run_frames(uint32_t frames, clock::time_point deadline) -> RunStats;

    /// True while the machine runs, also during a step that runs toward a goal
    [[nodiscard]]
    auto is_running() const noexcept -> bool {
        return running;
    }

    /// True while step_over(), step_out(), run_to(), or frame_advance() runs toward its goal
    [[nodiscard]]
    auto is_stepping() const noexcept -> bool {
        return running && (goal.kind != GoalKind::none);
    }

    /// Why execution stopped. StopReason::none while running.
    [[nodiscard]]
    auto last_stop() const noexcept -> const StopEvent& {
        return stop_event;
    }

    /// The newest stop event, once. A frontend uses it to log a stop or to show its location.
    [[nodiscard]]
    auto take_stop_event() noexcept -> std::optional<StopEvent>;

    /// The messages of the logpoints since the last call, oldest first
    [[nodiscard]]
    auto take_log_messages() -> std::vector<std::string>;


    //--------------------------------------------------------------------------------
    // Input
    //--------------------------------------------------------------------------------

    /// Set the state of a key (0x0-0xF). The history notes the change, so a replay presses the key at the same instruction.
    auto set_key(uint8_t key, bool down) -> void;


    //--------------------------------------------------------------------------------
    // History
    //--------------------------------------------------------------------------------

    /// Record the state at each frame end. Off clears the history.
    auto set_history_enabled(bool enable) -> void;

    [[nodiscard]]
    auto history_enabled() const noexcept -> bool {
        return recording;
    }

    /// Set the largest number of frames in the history. The oldest frames go first.
    auto set_history_length(size_t frames) -> void {
        state_history.set_capacity(frames);
    }

    [[nodiscard]]
    auto history() const noexcept -> const History& {
        return state_history;
    }

    /**
     * @brief Tell the debugger that the user changed the memory, the registers, or the seed
     *
     * @details A replay from a recorded state would not give the changed
     *          values, so the history starts again from the current state.
     */
    auto state_edited() -> void;


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

    /// Add a plain breakpoint, or give the one at the address
    auto add_breakpoint(uint16_t address) -> Breakpoint&;

    auto remove_breakpoint(uint16_t address) -> void {
        breakpoint_map.erase(address);
    }

    /// Add or remove a breakpoint. Returns true if the breakpoint now exists.
    auto toggle_breakpoint(uint16_t address) -> bool;

    /**
     * @brief Move the breakpoints to new addresses, for example after the source changed
     *
     * @details The settings of a breakpoint move with it. A breakpoint that the
     *          map does not name is removed. If two breakpoints move to the same
     *          address, the one with the lower old address stays.
     *
     * @param[in] moves  The new address of each old address
     */
    auto move_breakpoints(const std::map<uint16_t, uint16_t>& moves) -> void;

    auto clear_breakpoints() noexcept -> void {
        breakpoint_map.clear();
    }

    [[nodiscard]]
    auto has_breakpoint(uint16_t address) const -> bool {
        return breakpoint_map.contains(address);
    }

    /// The breakpoint at an address. nullptr if none.
    [[nodiscard]]
    auto find_breakpoint(uint16_t address) -> Breakpoint*;

    /// The first breakpoint in [address, address + size), for example in the bytes of one source line. nullptr if none.
    [[nodiscard]]
    auto first_breakpoint_in(uint16_t address, uint16_t size) const -> const Breakpoint*;

    /// True if a breakpoint is in [address, address + size)
    [[nodiscard]]
    auto has_breakpoint_in(uint16_t address, uint16_t size) const -> bool {
        return first_breakpoint_in(address, size) != nullptr;
    }

    /**
     * @brief Toggle the breakpoint of a range, for example the bytes of one source line
     *
     * @details If breakpoints exist in [address, address + size), all of them
     *          are removed. If not, a breakpoint is added at the address.
     *
     * @return True if a breakpoint now exists
     */
    auto toggle_breakpoint_in(uint16_t address, uint16_t size) -> bool;

    [[nodiscard]]
    auto breakpoints() const noexcept -> const std::map<uint16_t, Breakpoint>& {
        return breakpoint_map;
    }

    /// The addresses of all breakpoints
    [[nodiscard]]
    auto breakpoint_addresses() const -> std::set<uint16_t>;

    /// Set the condition of a breakpoint. An empty text removes it. Returns the error of a text that does not compile.
    auto set_breakpoint_condition(uint16_t address, std::string_view text) -> std::expected<void, std::string>;

    /// Set the log message of a breakpoint. An empty text makes it a breakpoint that stops again.
    auto set_breakpoint_log(uint16_t address, std::string_view text) -> std::expected<void, std::string>;

    /// Set the hit counts of all breakpoints and watchpoints to 0
    auto reset_hit_counts() noexcept -> void;


    //--------------------------------------------------------------------------------
    // Watchpoints and the break condition
    //--------------------------------------------------------------------------------

    /// Add a watchpoint. Returns its id.
    auto add_watchpoint(uint16_t address, uint16_t length, WatchAccess access) -> uint32_t;

    auto remove_watchpoint(uint32_t id) -> void;

    auto clear_watchpoints() noexcept -> void;

    /// The watchpoint with an id. nullptr if none.
    [[nodiscard]]
    auto find_watchpoint(uint32_t id) -> Watchpoint*;

    [[nodiscard]]
    auto watchpoints() const noexcept -> std::span<const Watchpoint> {
        return watchpoint_list;
    }

    /// Stop before each instruction while the condition is true. An empty text removes it.
    auto set_break_condition(std::string_view text) -> std::expected<void, std::string>;

    [[nodiscard]]
    auto break_condition() const noexcept -> const Compiled<DebugExpression>& {
        return global_condition;
    }


    //--------------------------------------------------------------------------------
    // Trace and profiler
    //--------------------------------------------------------------------------------

    /// Record each executed instruction in the trace
    auto set_tracing(bool enable) -> void;

    [[nodiscard]]
    auto trace() const noexcept -> const Trace& {
        return instruction_trace;
    }

    auto clear_trace() noexcept -> void {
        instruction_trace.clear();
    }

    /// Count the executions of each address and the reads and writes of each byte. A load or a reset clears the counts.
    auto set_profiling(bool enable) -> void;

    [[nodiscard]]
    auto profiler() const noexcept -> const Profiler& {
        return memory_profiler;
    }

    auto clear_profile() -> void {
        memory_profiler.clear(chip.state().memory.size());
    }

private:
    // Where a step that runs at normal speed stops
    enum class GoalKind : uint8_t {
        none,
        return_to,   // The PC is goal.pc at stack depth goal.sp, or the stack is less deep than goal.sp
        leave_frame, // The stack is less deep than goal.sp
        address,     // The PC is goal.pc
        frame_end,   // A frame ended
    };

    struct Goal {
        GoalKind kind = GoalKind::none;
        uint16_t pc   = 0;
        uint8_t  sp   = 0;
    };

    // The configuration that the history was recorded with. A replay with another configuration gives other states.
    struct HistoryConfig {
        Variant  variant = Variant::chip8;
        Quirks   quirks;
        uint32_t instructions_per_frame = 0;

        friend auto operator==(const HistoryConfig&, const HistoryConfig&) -> bool = default;
    };

    // Clear the stop state after a load or a reset, and run if a program exists
    auto start() -> void;

    // Clear the history and record the current state as its first state
    auto restart_history() -> void;

    // Record the state at a frame end. A change of the configuration starts the history again.
    auto record_history() -> void;

    // The configuration of the machine now
    [[nodiscard]]
    auto history_config() const noexcept -> HistoryConfig;

    // Restore a recorded state. The held keys stay, and the journal notes them.
    auto restore(MachineState&& past) -> void;

    // Run until the machine reaches a goal
    auto run_until(const Goal& target) noexcept -> void;

    // The reason to stop before the next cycle, if one exists
    [[nodiscard]]
    auto check_stop(bool use_breakpoints) -> std::optional<StopEvent>;

    // True if the breakpoint at the PC stops now. A logpoint writes its message and does not stop.
    [[nodiscard]]
    auto breakpoint_stops(Breakpoint& breakpoint, uint16_t pc) -> bool;

    // Run one cycle. While a tool is on, the tool sees the instruction before it runs.
    auto cycle() -> CycleResult;

    // Write a logpoint message, within the limit of the frame
    auto add_log_message(std::string message) -> void;

    // At a frame end or a stop, note the messages over the limit
    auto flush_log_limit() -> void;

    // Update the flag that tells if a tool is on
    auto update_tools() noexcept -> void;

    auto stop(const StopEvent& event) -> void;

    Chip8& chip;

    std::map<uint16_t, Breakpoint> breakpoint_map;
    std::vector<Watchpoint> watchpoint_list;
    uint32_t next_watchpoint_id = 1;
    Compiled<DebugExpression> global_condition;

    SymbolTable symbol_table;
    uint64_t symbol_version = 0;

    Trace instruction_trace;
    Profiler memory_profiler;

    History state_history;
    HistoryConfig recorded_config;
    bool recording = true;

    // A watchpoint, the break condition, the trace, or the profiler is on
    bool tools_on = false;

    bool leave_program_stops = true;
    bool running = false;

    // No instruction ran since execution resumed, so a breakpoint or an address goal at the PC does not stop it
    bool skip_stops_at_pc = false;

    Goal goal;
    StopEvent stop_event{};
    std::optional<StopEvent> unread_stop;

    std::vector<std::string> log_messages;
    uint32_t log_messages_in_frame = 0;
    uint32_t log_messages_dropped = 0;
};

} // namespace c8
