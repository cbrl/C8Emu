#include "debugger.h"

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
        case StopReason::fault:
            return std::format("Fault: {} at 0x{:04X} ({:04X})", to_string(event.fault.kind), event.fault.pc, event.fault.opcode);
        case StopReason::left_program:
            return std::format("The PC left the program at 0x{:04X}", event.pc);
    }
    return {};
}


Debugger::Debugger(Chip8& machine) noexcept : chip(machine) {
    start();
}


auto Debugger::load_program(std::span<const uint8_t> program, uint16_t origin) -> std::expected<void, std::string> {
    auto loaded = chip.load_program(program, origin);
    if (loaded) {
        start();
    }
    return loaded;
}


auto Debugger::reset() -> void {
    chip.reset();
    start();
}


auto Debugger::run() noexcept -> void {
    if (running || chip.program().empty()) {
        return;
    }

    running = true;
    skip_breakpoint = true;
    stop_event = {};
}


auto Debugger::pause() noexcept -> void {
    if (running) {
        stop({.reason = StopReason::user, .pc = chip.state().pc});
    }
}


auto Debugger::step_into() noexcept -> void {
    if (chip.program().empty()) {
        return;
    }

    running = false;

    // A pending vertical blank takes one idle cycle. Then one instruction runs.
    for (int attempt = 0; attempt < 2; ++attempt) {
        if (const auto event = check_stop(false)) {
            stop(*event);
            return;
        }

        const auto result = chip.cycle();
        if (result.fault.kind != Fault::none) {
            stop({.reason = StopReason::fault, .pc = result.fault.pc, .fault = result.fault});
            return;
        }
        if (result.executed) {
            break;
        }
    }

    skip_breakpoint = false;
    stop({.reason = StopReason::step, .pc = chip.state().pc});
}


auto Debugger::run_frames(uint32_t frames, clock::time_point deadline) noexcept -> RunStats {
    // Reading the clock costs more than a cycle, so check it only now and then
    static constexpr uint32_t clock_check_interval = 4096;

    auto stats = RunStats{};
    uint32_t until_clock_check = clock_check_interval;

    while (running && (stats.frames < frames)) {
        if (const auto event = check_stop(true)) {
            stop(*event);
            break;
        }

        const auto result = chip.cycle();
        if (result.fault.kind != Fault::none) {
            stop({.reason = StopReason::fault, .pc = result.fault.pc, .fault = result.fault});
            break;
        }

        if (result.executed) {
            ++stats.instructions;
            skip_breakpoint = false;
        }

        bool check_clock = false;
        if (result.frame_end) {
            ++stats.frames;
            check_clock = true;
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


auto Debugger::toggle_breakpoint(uint16_t address) -> bool {
    if (breakpoint_set.erase(address) != 0) {
        return false;
    }
    breakpoint_set.insert(address);
    return true;
}


auto Debugger::start() noexcept -> void {
    stop_event = {};
    unread_stop.reset();
    skip_breakpoint = false;
    running = !chip.program().empty();
}


auto Debugger::check_stop(bool use_breakpoints) const noexcept -> std::optional<StopEvent> {
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

    if (use_breakpoints && !skip_breakpoint && breakpoint_set.contains(pc)) {
        return StopEvent{.reason = StopReason::breakpoint, .pc = pc};
    }

    return std::nullopt;
}


auto Debugger::stop(const StopEvent& event) noexcept -> void {
    running = false;
    stop_event = event;
    unread_stop = event;
}

} // namespace c8
