#include "forms.h"

#include <algorithm>
#include <bit>
#include <initializer_list>

namespace c8 {

namespace {

using enum OperandKind;

constexpr auto form(
    std::string_view mnemonic,
    std::initializer_list<OperandKind> operands,
    uint16_t opcode,
    uint16_t mask,
    std::string_view pattern,
    std::string_view summary,
    bool y_copies_x = false) -> InstructionForm {

    auto result = InstructionForm{
        .mnemonic   = mnemonic,
        .opcode     = opcode,
        .mask       = mask,
        .pattern    = pattern,
        .summary    = summary,
        .y_copies_x = y_copies_x,
    };

    for (const auto kind : operands) {
        result.operand_list[result.operand_count] = kind;
        ++result.operand_count;
    }
    return result;
}


constexpr auto forms = std::array{
    form("sys",  {nnn},        0x0000, 0xF000, "0NNN", "Call a machine code routine. The emulator ignores it."),
    form("cls",  {},           0x00E0, 0xFFFF, "00E0", "Clear the screen"),
    form("ret",  {},           0x00EE, 0xFFFF, "00EE", "Return from a subroutine"),
    form("jmp",  {nnn},        0x1000, 0xF000, "1NNN", "Jump to nnn"),
    form("call", {nnn},        0x2000, 0xF000, "2NNN", "Call the subroutine at nnn"),
    form("se",   {vx, nn},     0x3000, 0xF000, "3XNN", "Skip the next instruction if vX == nn"),
    form("sne",  {vx, nn},     0x4000, 0xF000, "4XNN", "Skip the next instruction if vX != nn"),
    form("se",   {vx, vy},     0x5000, 0xF00F, "5XY0", "Skip the next instruction if vX == vY"),
    form("mov",  {vx, nn},     0x6000, 0xF000, "6XNN", "vX = nn"),
    form("add",  {vx, nn},     0x7000, 0xF000, "7XNN", "vX += nn. VF does not change."),
    form("mov",  {vx, vy},     0x8000, 0xF00F, "8XY0", "vX = vY"),
    form("or",   {vx, vy},     0x8001, 0xF00F, "8XY1", "vX |= vY"),
    form("and",  {vx, vy},     0x8002, 0xF00F, "8XY2", "vX &= vY"),
    form("xor",  {vx, vy},     0x8003, 0xF00F, "8XY3", "vX ^= vY"),
    form("add",  {vx, vy},     0x8004, 0xF00F, "8XY4", "vX += vY. VF = 1 on a carry."),
    form("sub",  {vx, vy},     0x8005, 0xF00F, "8XY5", "vX -= vY. VF = 0 on a borrow."),
    form("shr",  {vx, vy},     0x8006, 0xF00F, "8XY6", "vX = vY >> 1 (vX >> 1 with the shift quirk). VF = the bit shifted out."),
    form("shr",  {vx},         0x8006, 0xF00F, "8XX6", "shr vX, vX", true),
    form("subn", {vx, vy},     0x8007, 0xF00F, "8XY7", "vX = vY - vX. VF = 0 on a borrow."),
    form("shl",  {vx, vy},     0x800E, 0xF00F, "8XYE", "vX = vY << 1 (vX << 1 with the shift quirk). VF = the bit shifted out."),
    form("shl",  {vx},         0x800E, 0xF00F, "8XXE", "shl vX, vX", true),
    form("sne",  {vx, vy},     0x9000, 0xF00F, "9XY0", "Skip the next instruction if vX != vY"),
    form("mov",  {i, nnn},     0xA000, 0xF000, "ANNN", "I = nnn"),
    form("jmp",  {v0, nnn},    0xB000, 0xF000, "BNNN", "Jump to nnn + v0 (XNN + vX with the jump quirk)"),
    form("rnd",  {vx, nn},     0xC000, 0xF000, "CXNN", "vX = a random byte & nn"),
    form("drw",  {vx, vy, n},  0xD000, 0xF000, "DXYN", "Draw n rows of the sprite at I at (vX, vY). VF = 1 on a collision."),
    form("skp",  {vx},         0xE09E, 0xF0FF, "EX9E", "Skip the next instruction if the key in vX is held"),
    form("sknp", {vx},         0xE0A1, 0xF0FF, "EXA1", "Skip the next instruction if the key in vX is not held"),
    form("gdly", {vx},         0xF007, 0xF0FF, "FX07", "vX = the delay timer"),
    form("key",  {vx},         0xF00A, 0xF0FF, "FX0A", "Wait for a key press, then vX = the key"),
    form("sdly", {vx},         0xF015, 0xF0FF, "FX15", "The delay timer = vX"),
    form("ssnd", {vx},         0xF018, 0xF0FF, "FX18", "The sound timer = vX"),
    form("add",  {i, vx},      0xF01E, 0xF0FF, "FX1E", "I += vX"),
    form("font", {vx},         0xF029, 0xF0FF, "FX29", "I = the address of the font sprite for the digit in vX"),
    form("bcd",  {vx},         0xF033, 0xF0FF, "FX33", "Write the 3 decimal digits of vX at I, I+1, and I+2"),
    form("str",  {vx},         0xF055, 0xF0FF, "FX55", "Write v0 to vX at I. The load/store quirk sets the change to I."),
    form("ld",   {vx},         0xF065, 0xF0FF, "FX65", "Read v0 to vX from I. The load/store quirk sets the change to I."),
};

constexpr auto directives = std::array<std::string_view, 3>{"db", "dw", "org"};

} // namespace


auto instruction_forms() noexcept -> std::span<const InstructionForm> {
    return forms;
}


auto decode_form(uint16_t word) noexcept -> const InstructionForm* {
    const InstructionForm* best = nullptr;

    for (const auto& f : forms) {
        if (f.y_copies_x || ((word & f.mask) != f.opcode)) {
            continue;
        }
        if ((best == nullptr) || (std::popcount(f.mask) > std::popcount(best->mask))) {
            best = &f;
        }
    }

    return best;
}


auto is_mnemonic(std::string_view lowercase_word) noexcept -> bool {
    return std::ranges::any_of(forms, [&](const InstructionForm& f) { return f.mnemonic == lowercase_word; });
}


auto is_directive(std::string_view lowercase_word) noexcept -> bool {
    return std::ranges::find(directives, lowercase_word) != directives.end();
}


auto to_string(OperandKind kind) noexcept -> std::string_view {
    switch (kind) {
        case vx:  return "vX";
        case vy:  return "vY";
        case v0:  return "v0";
        case i:   return "i";
        case nnn: return "nnn";
        case nn:  return "nn";
        case n:   return "n";
    }
    return "?";
}


auto format_syntax(const InstructionForm& form) -> std::string {
    auto text = std::string{form.mnemonic};

    bool first = true;
    for (const auto kind : form.operands()) {
        text += first ? " " : ", ";
        text += to_string(kind);
        first = false;
    }

    return text;
}

} // namespace c8
