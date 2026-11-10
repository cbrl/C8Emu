#include "debugger.h"

#include "chip8/isa/decode.h"
#include "chip8/isa/isa.h"

#include <algorithm>
#include <format>
#include <utility>

namespace c8 {

auto describe(const StopEvent& event) -> std::string {
    switch (event.reason) {
        case StopReason::none:
            return {};
        case StopReason::user:
            return std::format("Paused at 0x{:04X}", event.pc);
        case StopReason::step:
            return std::format("Stepped to 0x{:04X}", event.pc);
        case StopReason::breakpoint:
            return std::format("Breakpoint at 0x{:04X}", event.pc);
        case StopReason::watchpoint:
            return std::format("Watchpoint: the instruction at 0x{:04X} {} 0x{:04X}", event.pc, event.write ? "writes" : "reads", event.address);
        case StopReason::condition:
            return std::format("The break condition is true at 0x{:04X}", event.pc);
        case StopReason::fault:
            return std::format("Fault: {} at 0x{:04X} ({:04X})", to_string(event.fault.kind), event.fault.pc, event.fault.opcode);
        case StopReason::left_program:
            return std::format("The PC left the program at 0x{:04X}", event.pc);
        case StopReason::exited:
            return std::format("The program exited (00FD) at 0x{:04X}", event.pc);
    }
    return {};
}


Debugger::Debugger(Chip8& machine) noexcept : chip(machine) {
    start();
}


auto Debugger::load_program(std::span<const uint8_t> program, uint16_t origin, std::optional<Variant> variant)
    -> std::expected<void, std::string> {

    auto loaded = chip.load_program(program, origin, variant);
    if (loaded) {
        start();
    }
    return loaded;
}


auto Debugger::reset() -> void {
    chip.reset();
    start();
}


auto Debugger::set_variant(Variant variant) -> std::expected<void, std::string> {
    if (variant == chip.variant()) {
        return {};
    }

    auto changed = chip.set_variant(variant);
    if (changed) {
        start();
    }
    return changed;
}


auto Debugger::apply_preset(const Preset& preset) -> std::expected<void, std::string> {
    const bool resets = preset.variant != chip.variant();

    auto applied = chip.apply_preset(preset);
    if (applied && resets) {
        start();
    }
    return applied;
}


auto Debugger::set_symbols(SymbolTable table) -> void {
    symbol_table = std::move(table);
    ++symbol_version;

    for (auto& [address, breakpoint] : breakpoint_map) {
        breakpoint.condition.compile(symbol_table);
        breakpoint.log.compile(symbol_table);
    }
    global_condition.compile(symbol_table);
    update_tools();
}


//--------------------------------------------------------------------------------
// Run control
//--------------------------------------------------------------------------------

auto Debugger::run() noexcept -> void {
    if (running || chip.program().empty()) {
        return;
    }

    running = true;
    skip_stops_at_pc = true;
    stop_event = {};
}


auto Debugger::pause() noexcept -> void {
    if (running) {
        running = false;
        goal = {};
        stop_event = {.reason = StopReason::user, .pc = chip.state().pc};
        unread_stop = stop_event;
    }
}


auto Debugger::step_into() -> void {
    if (chip.program().empty()) {
        return;
    }

    running = false;
    goal = {};

    // A pending vertical blank takes one idle cycle. Then one instruction runs.
    for (int attempt = 0; attempt < 2; ++attempt) {
        if (const auto event = check_stop(false)) {
            stop(*event);
            return;
        }

        const auto result = cycle();
        if (result.frame_end) {
            flush_log_limit();
        }
        if (result.fault.kind != Fault::none) {
            stop({.reason = StopReason::fault, .pc = result.fault.pc, .fault = result.fault});
            return;
        }
        if (result.exited) {
            stop({.reason = StopReason::exited, .pc = chip.state().pc});
            return;
        }
        if (result.executed) {
            break;
        }
    }

    skip_stops_at_pc = false;
    stop({.reason = StopReason::step, .pc = chip.state().pc});
}


auto Debugger::step_over() -> void {
    if (chip.program().empty()) {
        return;
    }

    const auto& state = chip.state();
    const auto op = isa::decode(state.read_word(state.pc), chip.variant());

    if ((op != isa::Op::call) && (op != isa::Op::key)) {
        step_into();
        return;
    }

    run_until({
        .kind = GoalKind::return_to,
        .pc   = static_cast<uint16_t>((state.pc + 2) & state.mask()),
        .sp   = state.sp,
    });
}


auto Debugger::step_out() noexcept -> void {
    if (chip.state().sp == 0) {
        return;
    }
    run_until({.kind = GoalKind::leave_frame, .sp = chip.state().sp});
}


auto Debugger::run_to(uint16_t address) noexcept -> void {
    run_until({.kind = GoalKind::address, .pc = address});
}


auto Debugger::frame_advance() noexcept -> void {
    run_until({.kind = GoalKind::frame_end});
}


auto Debugger::run_frames(uint32_t frames, clock::time_point deadline) -> RunStats {
    // Reading the clock costs more than a cycle, so check it only now and then
    static constexpr uint32_t clock_check_interval = 4096;

    auto stats = RunStats{};
    uint32_t until_clock_check = clock_check_interval;

    while (running && (stats.frames < frames)) {
        if (const auto event = check_stop(true)) {
            stop(*event);
            break;
        }

        const auto result = cycle();
        if (result.fault.kind != Fault::none) {
            stop({.reason = StopReason::fault, .pc = result.fault.pc, .fault = result.fault});
            break;
        }
        if (result.exited) {
            stop({.reason = StopReason::exited, .pc = chip.state().pc});
            break;
        }

        if (result.executed) {
            ++stats.instructions;
            skip_stops_at_pc = false;
        }

        bool check_clock = false;
        if (result.frame_end) {
            ++stats.frames;
            check_clock = true;
            flush_log_limit();

            if (goal.kind == GoalKind::frame_end) {
                stop({.reason = StopReason::step, .pc = chip.state().pc});
                break;
            }
        }
        if (--until_clock_check == 0) {
            check_clock = true;
        }

        if (check_clock) {
            until_clock_check = clock_check_interval;
            if ((stats.frames < frames) && (clock::now() >= deadline)) {
                stats.deadline_reached = true;
                break;
            }
        }
    }

    return stats;
}


auto Debugger::take_stop_event() noexcept -> std::optional<StopEvent> {
    return std::exchange(unread_stop, std::nullopt);
}


auto Debugger::take_log_messages() -> std::vector<std::string> {
    return std::exchange(log_messages, {});
}


//--------------------------------------------------------------------------------
// Breakpoints
//--------------------------------------------------------------------------------

auto Debugger::add_breakpoint(uint16_t address) -> Breakpoint& {
    return breakpoint_map.try_emplace(address).first->second;
}


auto Debugger::toggle_breakpoint(uint16_t address) -> bool {
    if (breakpoint_map.erase(address) != 0) {
        return false;
    }
    (void)add_breakpoint(address);
    return true;
}


auto Debugger::move_breakpoints(const std::map<uint16_t, uint16_t>& moves) -> void {
    auto moved = std::map<uint16_t, Breakpoint>{};

    // In order of the old address, so the lower old address wins a shared new address
    for (auto& [address, breakpoint] : breakpoint_map) {
        if (const auto it = moves.find(address); it != moves.end()) {
            moved.try_emplace(it->second, std::move(breakpoint));
        }
    }
    breakpoint_map = std::move(moved);
}


auto Debugger::find_breakpoint(uint16_t address) -> Breakpoint* {
    const auto it = breakpoint_map.find(address);
    return (it != breakpoint_map.end()) ? &it->second : nullptr;
}


auto Debugger::first_breakpoint_in(uint16_t address, uint16_t size) const -> const Breakpoint* {
    const auto it = breakpoint_map.lower_bound(address);
    if ((it == breakpoint_map.end()) || (it->first >= (uint32_t{address} + size))) {
        return nullptr;
    }
    return &it->second;
}


auto Debugger::toggle_breakpoint_in(uint16_t address, uint16_t size) -> bool {
    const uint32_t end = uint32_t{address} + size;
    bool removed = false;

    auto it = breakpoint_map.lower_bound(address);
    while ((it != breakpoint_map.end()) && (it->first < end)) {
        it = breakpoint_map.erase(it);
        removed = true;
    }

    if (removed) {
        return false;
    }
    (void)add_breakpoint(address);
    return true;
}


auto Debugger::breakpoint_addresses() const -> std::set<uint16_t> {
    auto addresses = std::set<uint16_t>{};
    for (const auto& [address, breakpoint] : breakpoint_map) {
        addresses.insert(address);
    }
    return addresses;
}


auto Debugger::set_breakpoint_condition(uint16_t address, std::string_view text) -> std::expected<void, std::string> {
    auto* breakpoint = find_breakpoint(address);
    if (breakpoint == nullptr) {
        return std::unexpected(std::format("No breakpoint at 0x{:04X}", address));
    }
    return breakpoint->condition.assign(text, symbol_table);
}


auto Debugger::set_breakpoint_log(uint16_t address, std::string_view text) -> std::expected<void, std::string> {
    auto* breakpoint = find_breakpoint(address);
    if (breakpoint == nullptr) {
        return std::unexpected(std::format("No breakpoint at 0x{:04X}", address));
    }
    return breakpoint->log.assign(text, symbol_table);
}


auto Debugger::reset_hit_counts() noexcept -> void {
    for (auto& [address, breakpoint] : breakpoint_map) {
        breakpoint.hit_count = 0;
    }
    for (auto& watchpoint : watchpoint_list) {
        watchpoint.hit_count = 0;
    }
}


//--------------------------------------------------------------------------------
// Watchpoints, the break condition, the trace, and the profiler
//--------------------------------------------------------------------------------

auto Debugger::add_watchpoint(uint16_t address, uint16_t length, WatchAccess access) -> uint32_t {
    const uint32_t id = next_watchpoint_id++;
    watchpoint_list.push_back(Watchpoint{
        .id      = id,
        .address = address,
        .length  = std::max<uint16_t>(length, 1),
        .access  = access,
    });
    update_tools();
    return id;
}


auto Debugger::remove_watchpoint(uint32_t id) -> void {
    std::erase_if(watchpoint_list, [id](const Watchpoint& watchpoint) { return watchpoint.id == id; });
    update_tools();
}


auto Debugger::clear_watchpoints() noexcept -> void {
    watchpoint_list.clear();
    update_tools();
}


auto Debugger::find_watchpoint(uint32_t id) -> Watchpoint* {
    const auto it = std::ranges::find(watchpoint_list, id, &Watchpoint::id);
    return (it != watchpoint_list.end()) ? &*it : nullptr;
}


auto Debugger::set_break_condition(std::string_view text) -> std::expected<void, std::string> {
    auto result = global_condition.assign(text, symbol_table);
    update_tools();
    return result;
}


auto Debugger::set_tracing(bool enable) -> void {
    if (enable) {
        instruction_trace.start();
    }
    else {
        instruction_trace.stop();
    }
    update_tools();
}


auto Debugger::set_profiling(bool enable) -> void {
    if (enable) {
        memory_profiler.start(chip.state().memory.size());
    }
    else {
        memory_profiler.stop();
    }
    update_tools();
}


//--------------------------------------------------------------------------------
// Internals
//--------------------------------------------------------------------------------

auto Debugger::start() -> void {
    stop_event = {};
    unread_stop.reset();
    skip_stops_at_pc = false;
    goal = {};
    running = !chip.program().empty();

    log_messages_in_frame = 0;
    log_messages_dropped = 0;

    // The counts of the old program or of the time before the reset mean nothing now
    memory_profiler.clear(chip.state().memory.size());
}


auto Debugger::run_until(const Goal& target) noexcept -> void {
    if (chip.program().empty()) {
        return;
    }

    running = true;
    skip_stops_at_pc = true;
    stop_event = {};
    goal = target;
}


auto Debugger::check_stop(bool use_breakpoints) -> std::optional<StopEvent> {
    const auto& state = chip.state();

    // The next cycle only ends the frame. No instruction runs.
    if (state.vblank_wait) {
        return std::nullopt;
    }

    // FX0A continues its wait. A breakpoint on it stops once, before the wait starts.
    if (state.in_key_wait()) {
        return std::nullopt;
    }

    const uint16_t pc = state.pc;

    if (leave_program_stops && ((pc < chip.program_origin()) || (pc >= chip.program_end()))) {
        return StopEvent{.reason = StopReason::left_program, .pc = pc};
    }

    if (use_breakpoints && !skip_stops_at_pc) {
        if (const auto it = breakpoint_map.find(pc); (it != breakpoint_map.end()) && breakpoint_stops(it->second, pc)) {
            return StopEvent{.reason = StopReason::breakpoint, .pc = pc};
        }

        if (tools_on) {
            if (!watchpoint_list.empty()) {
                const auto access = isa::memory_access(state, state.read_word(pc), chip.variant());
                if (const auto hit = find_watch_hit(watchpoint_list, access, state.mask())) {
                    ++watchpoint_list[hit->index].hit_count;
                    return StopEvent{.reason = StopReason::watchpoint, .pc = pc, .address = hit->address, .write = hit->write};
                }
            }

            if (global_condition.value && (global_condition.value->evaluate(state) != 0)) {
                return StopEvent{.reason = StopReason::condition, .pc = pc};
            }
        }
    }

    const auto reached_goal = StopEvent{.reason = StopReason::step, .pc = pc};

    switch (goal.kind) {
        case GoalKind::return_to: {
            if ((state.sp < goal.sp) || ((state.sp == goal.sp) && (pc == goal.pc))) {
                return reached_goal;
            }
            break;
        }
        case GoalKind::leave_frame: {
            if (state.sp < goal.sp) {
                return reached_goal;
            }
            break;
        }
        case GoalKind::address: {
            if (!skip_stops_at_pc && (pc == goal.pc)) {
                return reached_goal;
            }
            break;
        }
        case GoalKind::none:
        case GoalKind::frame_end: {
            break;
        }
    }

    return std::nullopt;
}


auto Debugger::breakpoint_stops(Breakpoint& breakpoint, uint16_t pc) -> bool {
    if (!breakpoint.enabled) {
        return false;
    }

    const auto& state = chip.state();

    // A condition with an error is not checked, so the breakpoint stops and the user sees the error
    if (breakpoint.condition.value && (breakpoint.condition.value->evaluate(state) == 0)) {
        return false;
    }

    ++breakpoint.hit_count;
    if (breakpoint.hit_count <= breakpoint.ignore_count) {
        return false;
    }

    if (breakpoint.log.value) {
        add_log_message(std::format("0x{:04X}: {}", pc, breakpoint.log.value->format(state)));
        return false;
    }

    return true;
}


auto Debugger::cycle() -> CycleResult {
    if (!tools_on) {
        return chip.cycle();
    }

    const auto& state = chip.state();
    const uint16_t pc = state.pc;

    // An idle cycle at a vertical blank runs no instruction
    if (!state.vblank_wait) {
        const uint16_t opcode = state.read_word(pc);

        // A key wait repeats the same FX0A, so the trace keeps only its first cycle
        if (!state.in_key_wait()) {
            instruction_trace.record(state, opcode);
        }
        memory_profiler.count_access(state, opcode, chip.variant());
    }

    const auto result = chip.cycle();
    if (result.executed) {
        memory_profiler.count_execution(pc & state.mask());
    }
    return result;
}


auto Debugger::add_log_message(std::string message) -> void {
    if (log_messages_in_frame < max_log_messages_per_frame) {
        log_messages.push_back(std::move(message));
        ++log_messages_in_frame;
    }
    else {
        ++log_messages_dropped;
    }
}


auto Debugger::flush_log_limit() -> void {
    if (log_messages_dropped > 0) {
        log_messages.push_back(std::format(
            "Logpoints: {} more messages in one frame were not written. A frame writes at most {}.",
            log_messages_dropped, max_log_messages_per_frame));
    }
    log_messages_in_frame = 0;
    log_messages_dropped = 0;
}


auto Debugger::update_tools() noexcept -> void {
    tools_on = !watchpoint_list.empty()
            || global_condition.value.has_value()
            || instruction_trace.is_recording()
            || memory_profiler.is_running();
}


auto Debugger::stop(const StopEvent& event) -> void {
    running = false;
    goal = {};
    stop_event = event;
    unread_stop = event;
    flush_log_limit();
}

} // namespace c8
