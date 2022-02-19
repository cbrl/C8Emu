#pragma once

#include <format>
#include <iostream>
#include <string>
#include <string_view>

#include "opcodes.h"
#include "util/strings.h"


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
inline auto to_string(const instruction& instr) -> std::string {
    using namespace std::string_literals;
    using namespace std::string_view_literals;

    static constexpr auto hex = "0123456789ABCDEF"sv;

    auto op = to_string(instr.opcode);
    replace(op,   "vx", std::format("v{}", hex.at(instr.x)));
    replace(op,   "vy", std::format("v{}", hex.at(instr.y)));
    replace(op, " nnn", std::format(" 0x{:03X}", instr.nnn));
    replace(op,  " nn", std::format(" 0x{:02X}", instr.nn));
    replace(op,   " n", std::format(" 0x{:01X}", instr.n));

    return op;
}
