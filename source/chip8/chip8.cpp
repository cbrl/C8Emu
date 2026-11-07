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


Chip8::Chip8(const Quirks& quirks, uint32_t instructions_per_frame) :
    quirk_set(quirks),
    ipf(std::clamp(instructions_per_frame, min_instructions_per_frame, max_instructions_per_frame)),
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
    // The keys are physical, so a reset does not release them
    const auto held_keys = machine.keys;

    machine = MachineState{};
    machine.keys = held_keys;

    std::ranges::copy(font, machine.memory.begin() + MachineState::font_address);
    std::ranges::copy(image, machine.memory.begin() + origin);

    machine.pc  = origin;
    machine.rng = next_seed();
}


auto Chip8::cycle() noexcept -> CycleResult {
    // After a draw with the display wait quirk, the rest of the frame is idle
    if (machine.vblank_wait) {
        end_frame();
        return {.frame_end = true};
    }

    const uint16_t pc = machine.pc;
    if ((static_cast<size_t>(pc) + 1) >= machine.memory.size()) {
        return {.fault = {Fault::pc_out_of_range, pc, 0}};
    }

    const uint16_t opcode = machine.read_word(pc);
    const auto result = isa::execute(machine, quirk_set, opcode);

    if (result.fault != Fault::none) {
        return {.fault = {result.fault, pc, opcode}};
    }

    ++machine.total_cycles;

    if ((result.effect == isa::Effect::drew) && quirk_set.display_wait) {
        machine.vblank_wait = true;
    }

    // A lower instructions-per-frame setting ends the current frame at once
    ++machine.cycle_in_frame;
    if (machine.cycle_in_frame >= ipf) {
        end_frame();
        return {.executed = true, .frame_end = true};
    }

    return {.executed = true};
}


auto Chip8::set_key(uint8_t key, bool down) noexcept -> void {
    key = static_cast<uint8_t>(key & 0xF);

    const auto bit = static_cast<uint16_t>(1u << key);
    machine.keys = down ? static_cast<uint16_t>(machine.keys | bit)
                        : static_cast<uint16_t>(machine.keys & ~bit);
}


auto Chip8::set_instructions_per_frame(uint32_t count) noexcept -> void {
    ipf = std::clamp(count, min_instructions_per_frame, max_instructions_per_frame);
}


auto Chip8::set_seed(std::optional<uint32_t> seed) noexcept -> void {
    fixed_seed = seed;
    machine.rng = next_seed();
}


auto Chip8::end_frame() noexcept -> void {
    machine.vblank_wait = false;
    machine.cycle_in_frame = 0;
    ++machine.frame_count;

    if (machine.delay_timer > 0) {
        --machine.delay_timer;
    }
    if (machine.sound_timer > 0) {
        --machine.sound_timer;
    }
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
