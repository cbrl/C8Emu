#include "analysis.h"

#include "chip8/isa/decode.h"
#include "chip8/machine_state.h"

#include <algorithm>
#include <format>

namespace c8 {

namespace {

// A jump table has at most this many 2-byte entries, because v0 is at most 0xFF
constexpr size_t max_table_entries = 128;

// The 4 KiB memory of CHIP-8 and SUPER-CHIP ends here
constexpr uint32_t small_memory_end = 0x1000;


// The instructions that only XO-CHIP has
constexpr auto is_xochip_op(isa::Op op) noexcept -> bool {
    using enum isa::Op;
    switch (op) {
        case scu:
        case str_range:
        case ld_range:
        case mov_i_long:
        case plane:
        case audio:
        case pitch:
            return true;
        default:
            return false;
    }
}

// The instructions that SUPER-CHIP adds to CHIP-8
constexpr auto is_schip_op(isa::Op op) noexcept -> bool {
    using enum isa::Op;
    switch (op) {
        case scd:
        case scr:
        case scl:
        case exit:
        case low:
        case high:
        case hfont:
        case sflags:
        case lflags:
            return true;
        default:
            return false;
    }
}

} // namespace


FlowAnalysis::FlowAnalysis(std::span<const uint8_t> program, uint32_t origin, Variant platform) :
    bytes(program),
    start(origin),
    variant(platform),
    roles(program.size(), ByteRole::unknown) {

    follow(origin);
    while (!pending.empty()) {
        const auto address = pending.back();
        pending.pop_back();
        visit(address);
    }
}


auto FlowAnalysis::offset_of(uint32_t address, size_t count) const -> std::optional<size_t> {
    if (address < start) {
        return std::nullopt;
    }
    const size_t offset = address - start;
    if ((offset + count) > bytes.size()) {
        return std::nullopt;
    }
    return offset;
}


auto FlowAnalysis::word_at(size_t offset) const -> uint16_t {
    return static_cast<uint16_t>((bytes[offset] << 8) | bytes[offset + 1]);
}


auto FlowAnalysis::follow(uint32_t address) -> void {
    pending.push_back(address);
}


auto FlowAnalysis::add_target(uint32_t address, TargetKind kind) -> void {
    const auto [it, inserted] = target_kinds.try_emplace(address, kind);
    if (!inserted) {
        it->second = std::max(it->second, kind);
    }
}


auto FlowAnalysis::visit(uint32_t address) -> void {
    const auto offset = offset_of(address, 2);
    if (!offset) {
        return;
    }

    // The path ran into data
    const uint16_t word = word_at(*offset);
    const auto op = isa::decode(word, variant);
    if ((word == 0x0000) || (op == isa::Op::invalid)) {
        return;
    }

    // All bytes of the instruction must be in the program and free. Otherwise
    // a path reached the address before, or the bytes belong to another instruction.
    const uint16_t size = isa::instruction_size(op);
    if (!offset_of(address, size)) {
        return;
    }
    for (size_t b = 0; b < size; ++b) {
        if (roles[*offset + b] != ByteRole::unknown) {
            return;
        }
    }

    roles[*offset] = ByteRole::instruction;
    for (size_t b = 1; b < size; ++b) {
        roles[*offset + b] = ByteRole::operand;
    }

    const uint32_t next   = address + size;
    const uint32_t target = word & 0xFFFu;

    if (isa::is_skip(op)) {
        // The skip moves over the whole next instruction, so over 4 bytes for F000 NNNN
        const auto next_offset = offset_of(next, 2);
        const bool long_next = has_xochip(variant) && next_offset && (word_at(*next_offset) == 0xF000);

        follow(next);
        follow(next + (long_next ? 4u : 2u));
        return;
    }

    switch (op) {
        case isa::Op::ret:
        case isa::Op::exit: {
            break;
        }
        case isa::Op::jmp: {
            // The interpreter extension of Hires CHIP-8 is not code. The program continues after it.
            if ((variant == Variant::chip8_hires) && (word == hires_entry_jump) && (address == MachineState::program_start)) {
                add_target(hires_program_start, TargetKind::jump);
                follow(hires_program_start);
                break;
            }
            add_target(target, TargetKind::jump);
            follow(target);
            break;
        }
        case isa::Op::call: {
            add_target(target, TargetKind::subroutine);
            follow(target);
            follow(next);
            break;
        }
        case isa::Op::mov_i_nnn: {
            add_target(target, TargetKind::data);
            follow(next);
            break;
        }
        case isa::Op::mov_i_long: {
            add_target(word_at(*offset + 2), TargetKind::data);
            follow(next);
            break;
        }
        case isa::Op::jmp_v0: {
            add_target(target, TargetKind::table);
            follow_table(target);
            break;
        }
        default: {
            follow(next);
            break;
        }
    }
}


auto FlowAnalysis::follow_table(uint32_t base) -> void {
    // With v0 = 0, the jump goes to the base itself
    follow(base);

    for (size_t entry = 0; entry < max_table_entries; ++entry) {
        const auto address = static_cast<uint32_t>(base + (entry * 2));
        const auto offset = offset_of(address, 2);

        if (!offset || (isa::decode(word_at(*offset), variant) != isa::Op::jmp)) {
            break;
        }
        follow(address);
    }
}


auto detect_platform(std::span<const uint8_t> program, uint16_t origin) -> PlatformGuess {
    const auto word_at = [&](size_t offset) {
        return static_cast<uint16_t>((program[offset] << 8) | program[offset + 1]);
    };

    if ((origin == MachineState::program_start) && (program.size() >= 2) && (word_at(0) == hires_entry_jump)) {
        return {Variant::chip8_hires, "the program starts with 1260"};
    }

    // XO-CHIP decodes the instructions of all other variants except Hires CHIP-8
    const auto flow = FlowAnalysis{program, origin, Variant::xochip};

    const auto first_use = [&](auto predicate) -> std::optional<std::string> {
        for (size_t offset = 0; offset < program.size(); ++offset) {
            if (flow.role(offset) != ByteRole::instruction) {
                continue;
            }
            const auto word = word_at(offset);
            if (predicate(isa::decode(word, Variant::xochip))) {
                return std::format("{:04X} at 0x{:04X}", word, origin + offset);
            }
        }
        return std::nullopt;
    };

    if (auto reason = first_use(is_xochip_op)) {
        return {Variant::xochip, std::move(*reason)};
    }

    const auto end = static_cast<uint32_t>(origin + program.size());
    if (end > small_memory_end) {
        return {Variant::xochip, std::format("the program ends at 0x{:04X}, past 4 KiB of memory", end)};
    }

    if (auto reason = first_use(is_schip_op)) {
        return {Variant::schip, std::move(*reason)};
    }

    return {Variant::chip8, "no SUPER-CHIP or XO-CHIP instructions"};
}

} // namespace c8
