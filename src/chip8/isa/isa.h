#pragma once

#include <functional>
#include <unordered_map>
#include "instruction/instruction.h"

class chip8;


/**
 * @class ISA
 * 
 * @brief Defines the functionality of the CHIP-8 instruction set
 * 
 * @details ISA is essentially a static class with one public function which
 *          executes a single cycle on an instance of a chip8.
 */
class ISA {
public:

    /**
     * @brief Execute a cycle
     *
     * @param[in] chip  An instance of chip8 to execute a cycle of
     */
    static auto execute_cycle(chip8& chip) -> void;

private:

    // Private constructor/destructor prevents this class from being constructed
    ISA() = default;
    ~ISA() = default;

    static auto increment_pc(chip8& chip) noexcept -> void;

    // 0x0---
    static auto cls(chip8& chip, instruction instr) -> void;     //0x00E0
    static auto ret(chip8& chip, instruction instr) -> void;     //0x00EE
    static auto sys_nnn(chip8& chip, instruction instr) -> void; //0x0nnn

    // 0x1nnn
    static auto jmp_nnn(chip8& chip, instruction instr) -> void;

    // 0x2nnn
    static auto call_nnn(chip8& chip, instruction instr) -> void;

    // 0x3xnn
    static auto se_vx_nn(chip8& chip, instruction instr) -> void;

    // 0x4xnn
    static auto sne_vx_nn(chip8& chip, instruction instr) -> void;

    // 0x5xy0
    static auto se_vx_vy(chip8& chip, instruction instr) -> void;

    // 0x6xnn
    static auto mov_vx_nn(chip8& chip, instruction instr) -> void;

    // 0x7xnn
    static auto add_vx_nn(chip8& chip, instruction instr) -> void;

    // 0x8---
    static auto mov_vx_vy(chip8& chip, instruction instr) -> void;  //0x8xy0
    static auto or_vx_vy(chip8& chip, instruction instr) -> void;   //0x8xy1
    static auto and_vx_vy(chip8& chip, instruction instr) -> void;  //0x8xy2
    static auto xor_vx_vy(chip8& chip, instruction instr) -> void;  //0x8xy3
    static auto add_vx_vy(chip8& chip, instruction instr) -> void;  //0x8xy4
    static auto sub_vx_vy(chip8& chip, instruction instr) -> void;  //0x8xy5
    static auto shr_vx(chip8& chip, instruction instr) -> void;     //0x8xy6
    static auto subn_vx_vy(chip8& chip, instruction instr) -> void; //0x8xy7
    static auto shl_vx(chip8& chip, instruction instr) -> void;     //0x8xyE
	
    // 0x9xy0
    static auto sne_vx_vy(chip8& chip, instruction instr) -> void;

    // 0xAnnn
    static auto mov_i_nnn(chip8& chip, instruction instr) -> void;

    // 0xBnnn
    static auto jmp_v0_nnn(chip8& chip, instruction instr) -> void;

    // 0xCxnn
    static auto rnd_vx_nn(chip8& chip, instruction instr) -> void;

    // 0xDxyn
    static auto drw_vx_vy_n(chip8& chip, instruction instr) -> void;

    // 0xE---
    static auto skp_vx(chip8& chip, instruction instr) -> void;  //0xEx9E
    static auto sknp_vx(chip8& chip, instruction instr) -> void; //0xExA1

    // 0xF---
    static auto gdly_vx(chip8& chip, instruction instr) -> void;   //0xFx07
    static auto key_vx(chip8& chip, instruction instr) -> void;    //0xFx0A
    static auto sdly_vx(chip8& chip, instruction instr) -> void;   //0xFx15
    static auto ssnd_vx(chip8& chip, instruction instr) -> void;   //0xFx18
    static auto add_i_vx(chip8& chip, instruction instr) -> void;  //0xFx1E
    static auto font_vx(chip8& chip, instruction instr) -> void;   //0xFx29
    static auto bcd_vx(chip8& chip, instruction instr) -> void;    //0xFx33
    static auto str_v0_vx(chip8& chip, instruction instr) -> void; //0xFx55
    static auto ld_v0_vx(chip8& chip, instruction instr) -> void;  //0xFx65


    // Map each opcode enum to the appropriate function
    static inline const std::unordered_map<Opcodes, std::function<void(chip8&, instruction)>> opcode_map = {
        {Opcodes::cls,         cls},
        {Opcodes::ret,         ret},
        {Opcodes::sys_nnn,     sys_nnn},
        {Opcodes::jmp_nnn,     jmp_nnn},
        {Opcodes::call_nnn,    call_nnn},
        {Opcodes::se_vx_nn,    se_vx_nn},
        {Opcodes::sne_vx_nn,   sne_vx_nn},
        {Opcodes::se_vx_vy,    se_vx_vy},
        {Opcodes::mov_vx_nn,   mov_vx_nn},
        {Opcodes::add_vx_nn,   add_vx_nn},
        {Opcodes::mov_vx_vy,   mov_vx_vy},
        {Opcodes::or_vx_vy,    or_vx_vy},
        {Opcodes::and_vx_vy,   and_vx_vy},
        {Opcodes::xor_vx_vy,   xor_vx_vy},
        {Opcodes::add_vx_vy,   add_vx_vy},
        {Opcodes::sub_vx_vy,   sub_vx_vy},
        {Opcodes::shr_vx,      shr_vx},
        {Opcodes::subn_vx_vy,  subn_vx_vy},
        {Opcodes::shl_vx,      shl_vx},
        {Opcodes::sne_vx_vy,   sne_vx_vy},
        {Opcodes::mov_i_nnn,   mov_i_nnn},
        {Opcodes::jmp_v0_nnn,  jmp_v0_nnn},
        {Opcodes::rnd_vx_nn,   rnd_vx_nn},
        {Opcodes::drw_vx_vy_n, drw_vx_vy_n},
        {Opcodes::skp_vx,      skp_vx},
        {Opcodes::sknp_vx,     sknp_vx},
        {Opcodes::gdly_vx,     gdly_vx},
        {Opcodes::key_vx,      key_vx},
        {Opcodes::sdly_vx,     sdly_vx},
        {Opcodes::ssnd_vx,     ssnd_vx},
        {Opcodes::add_i_vx,    add_i_vx},
        {Opcodes::font_vx,     font_vx},
        {Opcodes::bcd_vx,      bcd_vx},
        {Opcodes::str_v0_vx,   str_v0_vx},
        {Opcodes::ld_v0_vx,    ld_v0_vx},
    };
};
