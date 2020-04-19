#pragma once

#include "opcodes.h"
#include "util/strings.h"
#include <iostream>


/**
 * @struct instruction
 * @brief  Represents a CHIP-8 instruction, decomposed into the opcode and individual arguments.
 */
struct instruction {
public:
    instruction(uint16_t op) {
        data   = op;
        x      = (op & 0x0F00) >> 8;
        y      = (op & 0x00F0) >> 4;
        n      =  op & 0x000F;
        nn     =  op & 0x00FF;
        nnn    =  op & 0x0FFF;
        opcode = to_opcode(op);
    }

    instruction(uint8_t high, uint8_t low)
        : instruction((static_cast<uint16_t>(high) << 8) | low) {
    }


    // The packed representation of the instruction
    uint16_t data = 0;

    // The opcode without arguments
    Opcodes opcode =  Opcodes::invalid;

    // The arguments of the instruction
    uint16_t nnn : 12;
    uint8_t  nn;
    uint8_t  n : 4;
    uint8_t  x : 4;
    uint8_t  y : 4;
};


[[nodiscard]]
inline std::string to_string(const instruction& instr) {
    using namespace std::string_literals;

	static const char* hex = "0123456789ABCDEF";

    static char nnn_buf[7] = {};
    static char nn_buf[6] = {};
    static char n_buf[5] = {};

    std::sprintf(nnn_buf, " 0x%03X", instr.nnn);
    std::sprintf(nn_buf, " 0x%02X", instr.nn);
    std::sprintf(n_buf, " 0x%01X", instr.n);

    std::string op = to_string(instr.opcode);
    replace(op,   "vx", "v"s + hex[instr.x]);
    replace(op,   "vy", "v"s + hex[instr.y]);
    replace(op, " nnn",  nnn_buf);
    replace(op,  " nn",  nn_buf);
    replace(op,   " n",  n_buf);

    return op;
}
