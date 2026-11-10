#include "forms.h"

#include <algorithm>
#include <initializer_list>
#include <vector>

namespace c8 {

namespace {

using enum OperandKind;
using isa::Op;

constexpr auto form(
    std::string_view mnemonic,
    std::initializer_list<OperandKind> operands,
    uint16_t opcode,
    uint16_t mask,
    Op op,
    uint8_t variants,
    std::string_view pattern,
    std::string_view summary,
    bool y_copies_x = false) -> InstructionForm {

    auto result = InstructionForm{
        .mnemonic   = mnemonic,
        .opcode     = opcode,
        .mask       = mask,
        .op         = op,
        .variants   = variants,
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
    form("sys",    {nnn},          0x0000, 0xF000, Op::sys,        on_all,    "0NNN", "Call a machine code routine. The emulator ignores it."),
    form("scd",    {n},            0x00C0, 0xFFF0, Op::scd,        on_super,  "00CN", "Scroll the display down n pixels"),
    form("scu",    {n},            0x00D0, 0xFFF0, Op::scu,        on_xochip, "00DN", "Scroll the display up n pixels"),
    form("cls",    {},             0x00E0, 0xFFFF, Op::cls,        on_all,    "00E0", "Clear the screen (only the selected planes on XO-CHIP)"),
    form("ret",    {},             0x00EE, 0xFFFF, Op::ret,        on_all,    "00EE", "Return from a subroutine"),
    form("scr",    {},             0x00FB, 0xFFFF, Op::scr,        on_super,  "00FB", "Scroll the display right 4 pixels"),
    form("scl",    {},             0x00FC, 0xFFFF, Op::scl,        on_super,  "00FC", "Scroll the display left 4 pixels"),
    form("exit",   {},             0x00FD, 0xFFFF, Op::exit,       on_super,  "00FD", "Stop the program"),
    form("low",    {},             0x00FE, 0xFFFF, Op::low,        on_super,  "00FE", "Select lores mode (64x32) and clear the screen"),
    form("high",   {},             0x00FF, 0xFFFF, Op::high,       on_super,  "00FF", "Select hires mode (128x64) and clear the screen"),
    form("jmp",    {nnn},          0x1000, 0xF000, Op::jmp,        on_all,    "1NNN", "Jump to nnn"),
    form("call",   {nnn},          0x2000, 0xF000, Op::call,       on_all,    "2NNN", "Call the subroutine at nnn"),
    form("se",     {vx, nn},       0x3000, 0xF000, Op::se_vx_nn,   on_all,    "3XNN", "Skip the next instruction if vX == nn"),
    form("sne",    {vx, nn},       0x4000, 0xF000, Op::sne_vx_nn,  on_all,    "4XNN", "Skip the next instruction if vX != nn"),
    form("se",     {vx, vy},       0x5000, 0xF00F, Op::se_vx_vy,   on_all,    "5XY0", "Skip the next instruction if vX == vY"),
    form("str",    {vx, vy},       0x5002, 0xF00F, Op::str_range,  on_xochip, "5XY2", "Write vX to vY at I. X > Y writes in reverse order. I does not change."),
    form("ld",     {vx, vy},       0x5003, 0xF00F, Op::ld_range,   on_xochip, "5XY3", "Read vX to vY from I. X > Y reads in reverse order. I does not change."),
    form("mov",    {vx, nn},       0x6000, 0xF000, Op::mov_vx_nn,  on_all,    "6XNN", "vX = nn"),
    form("add",    {vx, nn},       0x7000, 0xF000, Op::add_vx_nn,  on_all,    "7XNN", "vX += nn. VF does not change."),
    form("mov",    {vx, vy},       0x8000, 0xF00F, Op::mov_vx_vy,  on_all,    "8XY0", "vX = vY"),
    form("or",     {vx, vy},       0x8001, 0xF00F, Op::or_vx_vy,   on_all,    "8XY1", "vX |= vY"),
    form("and",    {vx, vy},       0x8002, 0xF00F, Op::and_vx_vy,  on_all,    "8XY2", "vX &= vY"),
    form("xor",    {vx, vy},       0x8003, 0xF00F, Op::xor_vx_vy,  on_all,    "8XY3", "vX ^= vY"),
    form("add",    {vx, vy},       0x8004, 0xF00F, Op::add_vx_vy,  on_all,    "8XY4", "vX += vY. VF = 1 on a carry."),
    form("sub",    {vx, vy},       0x8005, 0xF00F, Op::sub_vx_vy,  on_all,    "8XY5", "vX -= vY. VF = 0 on a borrow."),
    form("shr",    {vx, vy},       0x8006, 0xF00F, Op::shr,        on_all,    "8XY6", "vX = vY >> 1 (vX >> 1 with the shift quirk). VF = the bit shifted out."),
    form("shr",    {vx},           0x8006, 0xF00F, Op::shr,        on_all,    "8XX6", "shr vX, vX", true),
    form("subn",   {vx, vy},       0x8007, 0xF00F, Op::subn_vx_vy, on_all,    "8XY7", "vX = vY - vX. VF = 0 on a borrow."),
    form("shl",    {vx, vy},       0x800E, 0xF00F, Op::shl,        on_all,    "8XYE", "vX = vY << 1 (vX << 1 with the shift quirk). VF = the bit shifted out."),
    form("shl",    {vx},           0x800E, 0xF00F, Op::shl,        on_all,    "8XXE", "shl vX, vX", true),
    form("sne",    {vx, vy},       0x9000, 0xF00F, Op::sne_vx_vy,  on_all,    "9XY0", "Skip the next instruction if vX != vY"),
    form("mov",    {i, nnn},       0xA000, 0xF000, Op::mov_i_nnn,  on_all,    "ANNN", "I = nnn"),
    form("jmp",    {v0, nnn},      0xB000, 0xF000, Op::jmp_v0,     on_all,    "BNNN", "Jump to nnn + v0 (XNN + vX with the jump quirk)"),
    form("rnd",    {vx, nn},       0xC000, 0xF000, Op::rnd,        on_all,    "CXNN", "vX = a random byte & nn"),
    form("drw",    {vx, vy, n},    0xD000, 0xF000, Op::drw,        on_all,    "DXYN", "Draw n rows of the sprite at I at (vX, vY). VF = 1 on a collision. On SUPER-CHIP and XO-CHIP, n = 0 draws a 16x16 sprite."),
    form("skp",    {vx},           0xE09E, 0xF0FF, Op::skp,        on_all,    "EX9E", "Skip the next instruction if the key in vX is held"),
    form("sknp",   {vx},           0xE0A1, 0xF0FF, Op::sknp,       on_all,    "EXA1", "Skip the next instruction if the key in vX is not held"),
    form("mov",    {i, long_kw, nnnn}, 0xF000, 0xFFFF, Op::mov_i_long, on_xochip, "F000 NNNN", "I = nnnn, a 16-bit address. The instruction is 4 bytes."),
    form("plane",  {nx},           0xF001, 0xF0FF, Op::plane,      on_xochip, "FN01", "Select the planes (0-3) that draw, clear, and scroll"),
    form("audio",  {},             0xF002, 0xFFFF, Op::audio,      on_xochip, "F002", "Copy the 16-byte audio pattern at I to the sound buffer"),
    form("gdly",   {vx},           0xF007, 0xF0FF, Op::gdly,       on_all,    "FX07", "vX = the delay timer"),
    form("key",    {vx},           0xF00A, 0xF0FF, Op::key,        on_all,    "FX0A", "Wait for a key press, then vX = the key"),
    form("sdly",   {vx},           0xF015, 0xF0FF, Op::sdly,       on_all,    "FX15", "The delay timer = vX"),
    form("ssnd",   {vx},           0xF018, 0xF0FF, Op::ssnd,       on_all,    "FX18", "The sound timer = vX"),
    form("add",    {i, vx},        0xF01E, 0xF0FF, Op::add_i_vx,   on_all,    "FX1E", "I += vX"),
    form("font",   {vx},           0xF029, 0xF0FF, Op::font,       on_all,    "FX29", "I = the address of the small font sprite for the digit in vX"),
    form("hfont",  {vx},           0xF030, 0xF0FF, Op::hfont,      on_super,  "FX30", "I = the address of the big font sprite (8x10) for the digit in vX"),
    form("bcd",    {vx},           0xF033, 0xF0FF, Op::bcd,        on_all,    "FX33", "Write the 3 decimal digits of vX at I, I+1, and I+2"),
    form("pitch",  {vx},           0xF03A, 0xF0FF, Op::pitch,      on_xochip, "FX3A", "Set the pitch of the audio pattern to vX. 64 plays 4000 bits per second."),
    form("str",    {vx},           0xF055, 0xF0FF, Op::str,        on_all,    "FX55", "Write v0 to vX at I. The load/store quirk sets the change to I."),
    form("ld",     {vx},           0xF065, 0xF0FF, Op::ld,         on_all,    "FX65", "Read v0 to vX from I. The load/store quirk sets the change to I."),
    form("sflags", {vx},           0xF075, 0xF0FF, Op::sflags,     on_super,  "FX75", "Save v0 to vX in the flag registers"),
    form("lflags", {vx},           0xF085, 0xF0FF, Op::lflags,     on_super,  "FX85", "Load v0 to vX from the flag registers"),
};

constexpr auto directives = std::array{
    DirectiveInfo{"db", "db value, ...",
        "Write bytes. A value can be an expression."},
    DirectiveInfo{"dw", "dw value, ...",
        "Write 16-bit words, high byte first. A label gives its address."},
    DirectiveInfo{"ds", "ds count [fill]",
        "Write count bytes of the fill value (0 if there is no fill). The count can use only the labels and constants before the line."},
    DirectiveInfo{"org", "org address",
        "Move forward to an address. Zeros fill the gap. The address can use only the labels and constants before the line."},
    DirectiveInfo{"align", "align size",
        "Move forward to a multiple of the size, a power of two. Zeros fill the gap."},
    DirectiveInfo{"equ", "name equ value",
        "Define a constant. The value can use labels and other constants, also the ones after the line."},
    DirectiveInfo{"platform", "platform name",
        "The platform: chip8, hires, schip, or xochip. It sets the instructions and the memory size. "
        "It must come before the first instruction, data, or org line. Without it, the platform of the machine applies."},
};

} // namespace


auto instruction_forms() noexcept -> std::span<const InstructionForm> {
    return forms;
}


auto directive_infos() noexcept -> std::span<const DirectiveInfo> {
    return directives;
}


auto find_directive(std::string_view lowercase_word) noexcept -> const DirectiveInfo* {
    const auto it = std::ranges::find(directives, lowercase_word, &DirectiveInfo::name);
    return (it != directives.end()) ? &*it : nullptr;
}


auto form_for_word(uint16_t word, Variant variant) noexcept -> const InstructionForm* {
    const auto op = isa::decode(word, variant);
    if (op == Op::invalid) {
        return nullptr;
    }

    const auto it = std::ranges::find_if(forms, [&](const InstructionForm& f) {
        return (f.op == op) && !f.y_copies_x && ((word & f.mask) == f.opcode) && f.available_on(variant);
    });
    return (it != forms.end()) ? &*it : nullptr;
}


auto is_mnemonic(std::string_view lowercase_word) noexcept -> bool {
    return std::ranges::any_of(forms, [&](const InstructionForm& f) { return f.mnemonic == lowercase_word; });
}


auto is_directive(std::string_view lowercase_word) noexcept -> bool {
    return find_directive(lowercase_word) != nullptr;
}


auto operand_fits(OperandKind kind, WordKind word) noexcept -> bool {
    switch (kind) {
        case vx:
        case vy:
        case v0:      return word == WordKind::v_register;
        case i:       return word == WordKind::i_register;
        case long_kw: return word == WordKind::long_keyword;
        case nnn:
        case nn:
        case n:
        case nx:
        case nnnn:    return (word != WordKind::v_register) && (word != WordKind::i_register) && (word != WordKind::long_keyword);
    }
    return false;
}


auto to_string(OperandKind kind) noexcept -> std::string_view {
    switch (kind) {
        case vx:      return "vX";
        case vy:      return "vY";
        case v0:      return "v0";
        case i:       return "i";
        case long_kw: return "long";
        case nnn:     return "nnn";
        case nn:      return "nn";
        case n:       return "n";
        case nx:      return "n";
        case nnnn:    return "nnnn";
    }
    return "?";
}


auto format_syntax(const InstructionForm& form) -> std::string {
    auto text = std::string{form.mnemonic};

    // The keyword `long` belongs to the address after it, so no comma follows it
    auto separator = std::string_view{" "};
    for (const auto kind : form.operands()) {
        text += separator;
        text += to_string(kind);
        separator = (kind == long_kw) ? " " : ", ";
    }

    return text;
}


auto platform_list(uint8_t variants) -> std::string {
    if ((variants & on_all) == on_all) {
        return "all platforms";
    }

    auto names = std::vector<std::string_view>{};
    for (const auto variant : all_variants) {
        if ((variants & (1u << static_cast<int>(variant))) != 0) {
            names.push_back(to_string(variant));
        }
    }

    auto text = std::string{};
    for (size_t idx = 0; idx < names.size(); ++idx) {
        if (idx > 0) {
            text += (idx + 1 == names.size()) ? " or " : ", ";
        }
        text += names[idx];
    }
    return text;
}

} // namespace c8
