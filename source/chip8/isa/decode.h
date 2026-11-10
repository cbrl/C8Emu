#pragma once

#include <cstddef>
#include <cstdint>

#include "chip8/variant.h"

namespace c8::isa {

/**
 * @brief The instructions of all CHIP-8 variants
 *
 * @details decode() gives the operation of a word for one variant. A word
 *          that the variant does not define is Op::invalid.
 */
enum class Op : uint8_t {
    invalid,

    // CHIP-8
    sys,        ///< 0NNN  machine code routine (ignored)
    cls,        ///< 00E0  clear the display (also 0230 in Hires CHIP-8)
    ret,        ///< 00EE  return
    jmp,        ///< 1NNN  jump
    call,       ///< 2NNN  call
    se_vx_nn,   ///< 3XNN
    sne_vx_nn,  ///< 4XNN
    se_vx_vy,   ///< 5XY0
    mov_vx_nn,  ///< 6XNN
    add_vx_nn,  ///< 7XNN
    mov_vx_vy,  ///< 8XY0
    or_vx_vy,   ///< 8XY1
    and_vx_vy,  ///< 8XY2
    xor_vx_vy,  ///< 8XY3
    add_vx_vy,  ///< 8XY4
    sub_vx_vy,  ///< 8XY5
    shr,        ///< 8XY6
    subn_vx_vy, ///< 8XY7
    shl,        ///< 8XYE
    sne_vx_vy,  ///< 9XY0
    mov_i_nnn,  ///< ANNN
    jmp_v0,     ///< BNNN
    rnd,        ///< CXNN
    drw,        ///< DXYN
    skp,        ///< EX9E
    sknp,       ///< EXA1
    gdly,       ///< FX07
    key,        ///< FX0A
    sdly,       ///< FX15
    ssnd,       ///< FX18
    add_i_vx,   ///< FX1E
    font,       ///< FX29
    bcd,        ///< FX33
    str,        ///< FX55
    ld,         ///< FX65

    // SUPER-CHIP
    scd,        ///< 00CN  scroll down N pixels
    scr,        ///< 00FB  scroll right 4 pixels
    scl,        ///< 00FC  scroll left 4 pixels
    exit,       ///< 00FD  exit the interpreter
    low,        ///< 00FE  lores mode (64x32)
    high,       ///< 00FF  hires mode (128x64)
    hfont,      ///< FX30  I = big font digit VX
    sflags,     ///< FX75  save V0..VX to the flag registers
    lflags,     ///< FX85  load V0..VX from the flag registers

    // XO-CHIP
    scu,        ///< 00DN  scroll up N pixels
    str_range,  ///< 5XY2  save VX..VY at I
    ld_range,   ///< 5XY3  load VX..VY from I
    mov_i_long, ///< F000 NNNN  I = NNNN (a 4-byte instruction)
    plane,      ///< FN01  select the drawing planes N
    audio,      ///< F002  load the 16-byte audio pattern from I
    pitch,      ///< FX3A  set the audio pitch to VX
};

/// The number of values of Op
inline constexpr size_t op_count = static_cast<size_t>(Op::pitch) + 1;


/// Decode an instruction word for a variant
[[nodiscard]]
constexpr auto decode(uint16_t word, Variant variant) noexcept -> Op {
    const auto n  = static_cast<uint8_t>(word & 0xF);
    const auto nn = static_cast<uint8_t>(word & 0xFF);

    const bool schip  = has_schip(variant);
    const bool xochip = has_xochip(variant);

    switch (word >> 12) {
        case 0x0: {
            if (word == 0x00E0) return Op::cls;
            if (word == 0x00EE) return Op::ret;
            if ((word == 0x0230) && (variant == Variant::chip8_hires)) return Op::cls;
            if (schip) {
                if ((word & 0xFFF0) == 0x00C0) return Op::scd;
                if (word == 0x00FB) return Op::scr;
                if (word == 0x00FC) return Op::scl;
                if (word == 0x00FD) return Op::exit;
                if (word == 0x00FE) return Op::low;
                if (word == 0x00FF) return Op::high;
            }
            if (xochip && ((word & 0xFFF0) == 0x00D0)) return Op::scu;
            return Op::sys;
        }
        case 0x1: return Op::jmp;
        case 0x2: return Op::call;
        case 0x3: return Op::se_vx_nn;
        case 0x4: return Op::sne_vx_nn;
        case 0x5: {
            if (n == 0x0) return Op::se_vx_vy;
            if (xochip && (n == 0x2)) return Op::str_range;
            if (xochip && (n == 0x3)) return Op::ld_range;
            return Op::invalid;
        }
        case 0x6: return Op::mov_vx_nn;
        case 0x7: return Op::add_vx_nn;
        case 0x8: {
            switch (n) {
                case 0x0: return Op::mov_vx_vy;
                case 0x1: return Op::or_vx_vy;
                case 0x2: return Op::and_vx_vy;
                case 0x3: return Op::xor_vx_vy;
                case 0x4: return Op::add_vx_vy;
                case 0x5: return Op::sub_vx_vy;
                case 0x6: return Op::shr;
                case 0x7: return Op::subn_vx_vy;
                case 0xE: return Op::shl;
                default:  return Op::invalid;
            }
        }
        case 0x9: return (n == 0) ? Op::sne_vx_vy : Op::invalid;
        case 0xA: return Op::mov_i_nnn;
        case 0xB: return Op::jmp_v0;
        case 0xC: return Op::rnd;
        case 0xD: return Op::drw;
        case 0xE: {
            if (nn == 0x9E) return Op::skp;
            if (nn == 0xA1) return Op::sknp;
            return Op::invalid;
        }
        case 0xF: {
            if (xochip) {
                if (word == 0xF000) return Op::mov_i_long;
                if (word == 0xF002) return Op::audio;
                if (nn == 0x01) return Op::plane;
                if (nn == 0x3A) return Op::pitch;
            }
            switch (nn) {
                case 0x07: return Op::gdly;
                case 0x0A: return Op::key;
                case 0x15: return Op::sdly;
                case 0x18: return Op::ssnd;
                case 0x1E: return Op::add_i_vx;
                case 0x29: return Op::font;
                case 0x33: return Op::bcd;
                case 0x55: return Op::str;
                case 0x65: return Op::ld;
                default:   break;
            }
            if (schip) {
                if (nn == 0x30) return Op::hfont;
                if (nn == 0x75) return Op::sflags;
                if (nn == 0x85) return Op::lflags;
            }
            return Op::invalid;
        }
        default: {
            return Op::invalid;
        }
    }
}


/// The size of an instruction in bytes: 4 for F000 NNNN, otherwise 2
[[nodiscard]]
constexpr auto instruction_size(Op op) noexcept -> uint16_t {
    return (op == Op::mov_i_long) ? 4 : 2;
}

/// True for an instruction that skips the next instruction on a condition
[[nodiscard]]
constexpr auto is_skip(Op op) noexcept -> bool {
    switch (op) {
        case Op::se_vx_nn:
        case Op::sne_vx_nn:
        case Op::se_vx_vy:
        case Op::sne_vx_vy:
        case Op::skp:
        case Op::sknp:
            return true;
        default:
            return false;
    }
}

} // namespace c8::isa
