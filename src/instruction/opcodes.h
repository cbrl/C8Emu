#pragma once

#include <cstdint>
#include <string>
#include <string_view>

/**
 * @enum Opcodes
 * 
 * @brief The set of opcodes in the CHIP-8 instruction set
 * 
 * @details The value of each element is that of the associated opcode, minus
 *          any arguments.
 */
enum class Opcodes : uint16_t {
    sys_nnn     = 0x0000,
    cls         = 0x00E0,
    ret         = 0x00EE,
    jmp_nnn     = 0x1000,
    call_nnn    = 0x2000,
    se_vx_nn    = 0x3000,
    sne_vx_nn   = 0x4000,
    se_vx_vy    = 0x5000,
    mov_vx_nn   = 0x6000,
    add_vx_nn   = 0x7000,
    mov_vx_vy   = 0x8000,
    or_vx_vy    = 0x8001,
    and_vx_vy   = 0x8002,
    xor_vx_vy   = 0x8003,
    add_vx_vy   = 0x8004,
    sub_vx_vy   = 0x8005,
    shr_vx      = 0x8006,
    subn_vx_vy  = 0x8007,
    shl_vx      = 0x800E,
    sne_vx_vy   = 0x9000,
    mov_i_nnn   = 0xA000,
    jmp_v0_nnn  = 0xB000,
    rnd_vx_nn   = 0xC000,
    drw_vx_vy_n = 0xD000,
    skp_vx      = 0xE09E,
    sknp_vx     = 0xE0A1,
    gdly_vx     = 0xF007,
    key_vx      = 0xF00A,
    sdly_vx     = 0xF015,
    ssnd_vx     = 0xF018,
    add_i_vx    = 0xF01E,
    font_vx     = 0xF029,
    bcd_vx      = 0xF033,
    str_v0_vx   = 0xF055,
    ld_v0_vx    = 0xF065,
    invalid     = 0xFFFF,
};


/**
 * @brief Convert an @ref Opcode to a string
 * 
 * @param[in] op  The opcode value
 * 
 * @return The string representation of the opcode
 */
[[nodiscard]]
auto to_string(Opcodes op) -> std::string;


/**
 * @brief Convert a string to an @ref Opcode
 * 
 * @param[in] op  The string representation of the opcode
 * 
 * @return The opcode value
 */
[[nodiscard]]
auto to_opcode(std::string_view op) -> Opcodes;


/**
 * @brief Convert a CHIP-8 instruction to an @ref Opcode
 * 
 * @param[in] instruction  The 16-bit CHIP-8 instruction
 * 
 * @return The opcode value
 */
[[nodiscard]]
auto to_opcode(uint16_t instruction) -> Opcodes;
