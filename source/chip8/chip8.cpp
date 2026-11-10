#include "chip8.h"
#include "isa/isa.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <format>
#include <utility>

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

// The big digit sprites of SUPER-CHIP and XO-CHIP, 10 bytes each
constexpr auto big_font = std::array<uint8_t, 160>{
    0xFF, 0xFF, 0xC3, 0xC3, 0xC3, 0xC3, 0xC3, 0xC3, 0xFF, 0xFF, // 0
    0x18, 0x78, 0x78, 0x18, 0x18, 0x18, 0x18, 0x18, 0xFF, 0xFF, // 1
    0xFF, 0xFF, 0x03, 0x03, 0xFF, 0xFF, 0xC0, 0xC0, 0xFF, 0xFF, // 2
    0xFF, 0xFF, 0x03, 0x03, 0xFF, 0xFF, 0x03, 0x03, 0xFF, 0xFF, // 3
    0xC3, 0xC3, 0xC3, 0xC3, 0xFF, 0xFF, 0x03, 0x03, 0x03, 0x03, // 4
    0xFF, 0xFF, 0xC0, 0xC0, 0xFF, 0xFF, 0x03, 0x03, 0xFF, 0xFF, // 5
    0xFF, 0xFF, 0xC0, 0xC0, 0xFF, 0xFF, 0xC3, 0xC3, 0xFF, 0xFF, // 6
    0xFF, 0xFF, 0x03, 0x03, 0x06, 0x0C, 0x18, 0x18, 0x18, 0x18, // 7
    0xFF, 0xFF, 0xC3, 0xC3, 0xFF, 0xFF, 0xC3, 0xC3, 0xFF, 0xFF, // 8
    0xFF, 0xFF, 0xC3, 0xC3, 0xFF, 0xFF, 0x03, 0x03, 0xFF, 0xFF, // 9
    0x7E, 0xFF, 0xC3, 0xC3, 0xC3, 0xFF, 0xFF, 0xC3, 0xC3, 0xC3, // A
    0xFC, 0xFC, 0xC3, 0xC3, 0xFC, 0xFC, 0xC3, 0xC3, 0xFC, 0xFC, // B
    0x3C, 0xFF, 0xC3, 0xC0, 0xC0, 0xC0, 0xC0, 0xC3, 0xFF, 0x3C, // C
    0xFC, 0xFE, 0xC3, 0xC3, 0xC3, 0xC3, 0xC3, 0xC3, 0xFE, 0xFC, // D
    0xFF, 0xFF, 0xC0, 0xC0, 0xFF, 0xFF, 0xC0, 0xC0, 0xFF, 0xFF, // E
    0xFF, 0xFF, 0xC0, 0xC0, 0xFF, 0xFF, 0xC0, 0xC0, 0xC0, 0xC0, // F
};

} // namespace


Chip8::Chip8(Variant variant, const Quirks& quirks, uint32_t instructions_per_frame) :
    machine_variant(variant),
    quirk_set(quirks),
    ipf(std::clamp(instructions_per_frame, min_instructions_per_frame, max_instructions_per_frame)),
    seed_counter(static_cast<uint64_t>(std::chrono::steady_clock::now().time_since_epoch().count())) {
    reset();
}


auto Chip8::load_program(std::span<const uint8_t> program, uint16_t new_origin, std::optional<Variant> variant)
    -> std::expected<void, std::string> {

    if (program.empty()) {
        return std::unexpected(std::string{"The program is empty"});
    }

    const auto new_variant = variant.value_or(machine_variant);
    const auto capacity = max_program_size(new_variant, new_origin);
    if (program.size() > capacity) {
        return std::unexpected(std::format(
            "The program is too large for {}: {} bytes, but only {} bytes fit at 0x{:03X}",
            to_string(new_variant), program.size(), capacity, new_origin
        ));
    }

    // The flag registers belong to one program
    if (!std::ranges::equal(program, image) || (new_origin != origin)) {
        machine.flags = {};
    }

    image.assign(program.begin(), program.end());
    origin = new_origin;
    machine_variant = new_variant;
    reset();

    return {};
}


auto Chip8::reset() -> void {
    // The keys are physical, so a reset does not release them. The flag
    // registers keep their values, as on the HP-48 calculator.
    const auto held_keys = machine.keys;
    const auto flags = machine.flags;

    machine = MachineState{};
    machine.keys = held_keys;
    machine.flags = flags;
    machine.memory.resize(memory_size(machine_variant));

    if (machine_variant == Variant::chip8_hires) {
        machine.display.resize(64, 64);
    }

    std::ranges::copy(font, machine.memory.begin() + MachineState::font_address);
    if (has_schip(machine_variant)) {
        std::ranges::copy(big_font, machine.memory.begin() + MachineState::big_font_address);
    }
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
    const auto result = isa::execute(machine, quirk_set, machine_variant, opcode);

    if (result.fault != Fault::none) {
        return {.fault = {result.fault, pc, opcode}};
    }
    if (result.effect == isa::Effect::exited) {
        return {.exited = true};
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


auto Chip8::set_variant(Variant variant) -> std::expected<void, std::string> {
    if (variant == machine_variant) {
        return {};
    }

    const auto capacity = max_program_size(variant, origin);
    if (image.size() > capacity) {
        return std::unexpected(std::format(
            "The program does not fit in {} memory: {} bytes, but only {} bytes fit at 0x{:03X}",
            to_string(variant), image.size(), capacity, origin
        ));
    }

    machine_variant = variant;
    reset();
    return {};
}


auto Chip8::apply_preset(const Preset& preset) -> std::expected<void, std::string> {
    auto changed = set_variant(preset.variant);
    if (!changed) {
        return changed;
    }

    quirk_set = preset.quirks;
    set_instructions_per_frame(preset.instructions_per_frame);
    return {};
}


auto Chip8::set_instructions_per_frame(uint32_t count) noexcept -> void {
    ipf = std::clamp(count, min_instructions_per_frame, max_instructions_per_frame);
}


auto Chip8::save_state() const -> SaveState {
    return SaveState{
        .variant = machine_variant,
        .quirks  = quirk_set,
        .instructions_per_frame = ipf,
        .origin  = origin,
        .program = image,
        .state   = machine,
    };
}


auto Chip8::load_state(SaveState save) -> std::expected<void, std::string> {
    const auto& state = save.state;
    const auto variant = save.variant;
    const auto error = [](std::string message) { return std::unexpected(std::move(message)); };

    if (save.program.empty()) {
        return error("The save state has no program");
    }
    if (save.program.size() > max_program_size(variant, save.origin)) {
        return error(std::format("The program of the save state does not fit in {} memory", to_string(variant)));
    }
    if (state.memory.size() != memory_size(variant)) {
        return error(std::format("The memory size {} does not match {}", state.memory.size(), to_string(variant)));
    }
    if (state.pc >= state.memory.size()) {
        return error(std::format("The PC 0x{:04X} is outside the {} memory", state.pc, to_string(variant)));
    }
    if (state.sp > MachineState::stack_depth) {
        return error(std::format("The stack depth {} is more than {}", state.sp, MachineState::stack_depth));
    }
    if (state.rng == 0) {
        return error("The random number state is 0");
    }
    if ((save.instructions_per_frame < min_instructions_per_frame) || (save.instructions_per_frame > max_instructions_per_frame)) {
        return error(std::format("The speed of {} instructions per frame is not valid", save.instructions_per_frame));
    }

    // Each variant has its own display sizes
    const auto width = state.display.width();
    const auto height = state.display.height();
    const bool lores = (width == 64) && (height == 32);
    const bool display_fits = [&] {
        switch (variant) {
            case Variant::chip8:       return lores;
            case Variant::chip8_hires: return (width == 64) && (height == 64);
            case Variant::schip:
            case Variant::xochip:      return lores || ((width == 128) && (height == 64));
        }
        return false;
    }();
    if (!display_fits) {
        return error(std::format("The display size {}x{} does not match {}", width, height, to_string(variant)));
    }

    // The keys are physical, so the held keys stay
    const auto held_keys = machine.keys;

    machine_variant = variant;
    quirk_set = save.quirks;
    ipf = save.instructions_per_frame;
    origin = save.origin;
    image = std::move(save.program);
    machine = std::move(save.state);
    machine.keys = held_keys;

    return {};
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
