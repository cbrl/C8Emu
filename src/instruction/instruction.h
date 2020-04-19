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

    std::string op = to_string(instr.opcode);
    replace(op,    "vx",      "v"s + hex[instr.x]);
    replace(op,    "vy",      "v"s + hex[instr.y]);
    replace(op,   " n", ' ' + std::to_string(instr.n));
    replace(op,  " nn", ' ' + std::to_string(instr.nn));
    replace(op, " nnn", ' ' + std::to_string(instr.nnn));

    return op;

    /* Alternate replace method
    const std::vector<std::string> op_split = Split(op, " ");
    std::string instruction = op_split[0];
    for (size_t i = 1; i < op_split.size(); ++i) {
        instruction += ' ';

        if (op_split[i] == "vx") {
            instruction += 'v' + std::to_string(instr.x);
        }
        else if (op_split[i] == "vy") {
            instruction += 'v' + std::to_string(instr.y);
        }
        else if (op_split[i] == "n") {
            instruction += std::to_string(instr.n);
        }
        else if (op_split[i] == "nn") {
            instruction += std::to_string(instr.nn);
        }
        else if (op_split[i] == "nnn") {
            instruction += std::to_string(instr.nnn);
        }
        else {
            instruction += op_split[i];
        }
    }
    */
}
