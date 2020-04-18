#pragma once

#include <map>
#include <functional>
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
    static void execute_cycle(chip8& chip);

private:

    // Private constructor/destructor prevents this class from being constructed
    ISA() = default;
    ~ISA() = default;

    static void increment_pc(chip8& chip) noexcept;

    // 0x0---
    static void cls(chip8& chip, instruction instr);     //0x00E0
    static void ret(chip8& chip, instruction instr);     //0x00EE
    static void sys_nnn(chip8& chip, instruction instr); //0x0nnn

    // 0x1nnn
    static void jmp_nnn(chip8& chip, instruction instr);

    // 0x2nnn
    static void call_nnn(chip8& chip, instruction instr);

    // 0x3xnn
    static void se_vx_nn(chip8& chip, instruction instr);

    // 0x4xnn
    static void sne_vx_nn(chip8& chip, instruction instr);

    // 0x5xy0
    static void se_vx_vy(chip8& chip, instruction instr);

    // 0x6xnn
    static void mov_vx_nn(chip8& chip, instruction instr);

    // 0x7xnn
    static void add_vx_nn(chip8& chip, instruction instr);

    // 0x8---
    static void mov_vx_vy(chip8& chip, instruction instr);  //0x8xy0
    static void or_vx_vy(chip8& chip, instruction instr);   //0x8xy1
    static void and_vx_vy(chip8& chip, instruction instr);  //0x8xy2
    static void xor_vx_vy(chip8& chip, instruction instr);  //0x8xy3
    static void add_vx_vy(chip8& chip, instruction instr);  //0x8xy4
    static void sub_vx_vy(chip8& chip, instruction instr);  //0x8xy5
    static void shr_vx(chip8& chip, instruction instr);     //0x8xy6
    static void subn_vx_vy(chip8& chip, instruction instr); //0x8xy7
    static void shl_vx(chip8& chip, instruction instr);     //0x8xyE
	
    // 0x9xy0
    static void sne_vx_vy(chip8& chip, instruction instr);

    // 0xAnnn
    static void mov_i_nnn(chip8& chip, instruction instr);

    // 0xBnnn
    static void jmp_v0_nnn(chip8& chip, instruction instr);

    // 0xCxnn
    static void rnd_vx_nn(chip8& chip, instruction instr);

    // 0xDxyn
    static void drw_vx_vy_n(chip8& chip, instruction instr);

    // 0xE---
    static void skp_vx(chip8& chip, instruction instr);  //0xEx9E
    static void sknp_vx(chip8& chip, instruction instr); //0xExA1

    // 0xF---
    static void gdly_vx(chip8& chip, instruction instr);   //0xFx07
    static void key_vx(chip8& chip, instruction instr);    //0xFx0A
    static void sdly_vx(chip8& chip, instruction instr);   //0xFx15
    static void ssnd_vx(chip8& chip, instruction instr);   //0xFx18
    static void add_i_vx(chip8& chip, instruction instr);  //0xFx1E
    static void font_vx(chip8& chip, instruction instr);   //0xFx29
    static void bcd_vx(chip8& chip, instruction instr);    //0xFx33
    static void str_v0_vx(chip8& chip, instruction instr); //0xFx55
    static void ld_v0_vx(chip8& chip, instruction instr);  //0xFx65


    // Map each opcode enum to the appropriate function
    static inline const std::map<Opcodes, std::function<void(chip8&, instruction)>> opcode_map = {
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
