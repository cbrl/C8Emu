#include "chip8.h"
#include "isa/isa.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <format>

namespace c8 {
namespace {

// The hexadecimal digit sprites, 5 bytes each
constexpr auto font = std::array<uint8_t, 80>{
    0xF0, 0x90, 0x90, 0x90, 0xF0, // 0
    0x20, 0x60, 0x20, 0x20, 0x70, // 1
    0xF0, 0x10, 0xF0, 0x80, 0xF0, // 2
    0xF0, 0x10, 0xF0, 0x10, 0xF0, // 3
    0x90, 0x90, 0xF0, 0x10, 0x10, // 4
    0xF0, 0x80, 0xF0, 0x10, 0xF0, // 5
    0xF0, 0x80, 0xF0, 0x90, 0xF0, // 6
    0xF0, 0x10, 0x20, 0x40, 0x40, // 7
    0xF0, 0x90, 0xF0, 0x90, 0xF0, // 8
    0xF0, 0x90, 0xF0, 0x10, 0xF0, // 9
    0xF0, 0x90, 0xF0, 0x90, 0x90, // A
    0xE0, 0x90, 0xE0, 0x90, 0xE0, // B
    0xF0, 0x80, 0x80, 0x80, 0xF0, // C
    0xE0, 0x90, 0x90, 0x90, 0xE0, // D
    0xF0, 0x80, 0xF0, 0x80, 0xF0, // E
    0xF0, 0x80, 0xF0, 0x80, 0x80  // F
};

} // namespace


auto describe(const StopEvent& event) -> std::string {
    switch (event.reason) {
        case StopReason::none:
            return {};
        case StopReason::user:
            return std::format("Paused at 0x{:04X}", event.pc);
        case StopReason::breakpoint:
            return std::format("Breakpoint at 0x{:04X}", event.pc);
        case StopReason::fault:
            return std::format("Fault: {} at 0x{:04X} ({:04X})", to_string(event.fault.kind), event.fault.pc, event.fault.opcode);
        case StopReason::left_program:
            return std::format("The PC left the program at 0x{:04X}", event.pc);
        case StopReason::key_wait:
            return std::format("Waiting for a key at 0x{:04X}", event.pc);
    }
    return {};
}


Chip8::Chip8(const Quirks& quirks) :
    quirk_set(quirks),
    seed_counter(static_cast<uint64_t>(std::chrono::steady_clock::now().time_since_epoch().count())) {
    reset();
}


auto Chip8::load_program(std::span<const uint8_t> program, uint16_t new_origin) -> std::expected<void, std::string> {
    if (program.empty()) {
        return std::unexpected(std::string{"The program is empty"});
    }

    const auto capacity = max_program_size(new_origin);
    if (program.size() > capacity) {
        return std::unexpected(std::format(
            "The program is too large: {} bytes, but only {} bytes fit at 0x{:03X}",
            program.size(), capacity, new_origin
        ));
    }

    image.assign(program.begin(), program.end());
    origin = new_origin;
    reset();

    return {};
}


auto Chip8::reset() -> void {
    machine = MachineState{};

    std::ranges::copy(font, machine.memory.begin() + MachineState::font_address);
    std::ranges::copy(image, machine.memory.begin() + origin);

    machine.pc  = origin;
    machine.rng = next_seed();

    key_wait_register.reset();
    skip_breakpoint = false;
    stop_event = {};

    // Run the program at once. Without a program there is nothing to run.
    paused = image.empty();
}


auto Chip8::pause() noexcept -> void {
    // Keep the first reason, unless the machine only waits for a key
    if (paused && stop_event.reason != StopReason::key_wait) {
        return;
    }

    // A key press must not resume a paused machine. FX0A runs again after resume().
    key_wait_register.reset();
    stop({.reason = StopReason::user, .pc = machine.pc});
}


auto Chip8::resume() noexcept -> void {
    key_wait_register.reset();
    stop_event = {};
    skip_breakpoint = true;
    paused = false;
}


auto Chip8::step() noexcept -> void {
    key_wait_register.reset();
    skip_breakpoint = true;
    execute_cycle();
}


auto Chip8::run_cycle() noexcept -> void {
    if (!paused) {
        execute_cycle();
    }
}


auto Chip8::tick_timers() noexcept -> void {
    if (machine.delay_timer > 0) {
        --machine.delay_timer;
    }
    if (machine.sound_timer > 0) {
        --machine.sound_timer;
    }
}


auto Chip8::set_key(uint8_t key, bool down) noexcept -> void {
    key = static_cast<uint8_t>(key & 0xF);

    const auto bit = static_cast<uint16_t>(1u << key);
    machine.keys = down ? static_cast<uint16_t>(machine.keys | bit)
                        : static_cast<uint16_t>(machine.keys & ~bit);

    if (!down || !key_wait_register) {
        return;
    }

    // Complete FX0A only if the PC still points at the same FX0A instruction
    const auto reg = *key_wait_register;
    const auto expected = static_cast<uint16_t>(0xF00A | (reg << 8));
    key_wait_register.reset();

    if (machine.read_word(machine.pc) != expected) {
        return;
    }

    machine.v[reg] = key;
    machine.pc = static_cast<uint16_t>(machine.pc + 2);
    stop_event = {};
    paused = false;
}


auto Chip8::set_clock_rate(uint32_t rate) noexcept -> void {
    clock_rate = std::clamp(rate, min_clock_rate, max_clock_rate);
}


auto Chip8::set_seed(std::optional<uint32_t> seed) noexcept -> void {
    fixed_seed = seed;
    machine.rng = next_seed();
}


auto Chip8::toggle_breakpoint(uint16_t address) -> bool {
    if (breakpoint_set.erase(address) != 0) {
        return false;
    }
    breakpoint_set.insert(address);
    return true;
}


auto Chip8::execute_cycle() noexcept -> void {
    const uint16_t pc = machine.pc;

    if (pc < origin || pc >= program_end()) {
        stop({.reason = StopReason::left_program, .pc = pc});
        return;
    }

    if ((static_cast<size_t>(pc) + 1) >= machine.memory.size()) {
        stop({.reason = StopReason::fault, .pc = pc, .fault = {Fault::pc_out_of_range, pc, 0}});
        return;
    }

    if (!skip_breakpoint && breakpoint_set.contains(pc)) {
        stop({.reason = StopReason::breakpoint, .pc = pc});
        return;
    }
    skip_breakpoint = false;

    const uint16_t opcode = machine.read_word(pc);
    const auto result = isa::execute(machine, quirk_set, opcode);

    if (result.fault != Fault::none) {
        stop({.reason = StopReason::fault, .pc = pc, .fault = {result.fault, pc, opcode}});
        return;
    }

    if (result.effect == isa::Effect::waiting) {
        key_wait_register = static_cast<uint8_t>((opcode >> 8) & 0xF);
        stop({.reason = StopReason::key_wait, .pc = pc});
    }
}


auto Chip8::stop(const StopEvent& event) noexcept -> void {
    stop_event = event;
    paused = true;
}


auto Chip8::next_seed() noexcept -> uint32_t {
    if (fixed_seed) {
        return (*fixed_seed != 0) ? *fixed_seed : 1;
    }

    // splitmix64
    seed_counter += 0x9E3779B97F4A7C15ull;
    uint64_t z = seed_counter;
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
    z ^= z >> 31;

    const auto seed = static_cast<uint32_t>(z);
    return (seed != 0) ? seed : 1;
}

} // namespace c8
