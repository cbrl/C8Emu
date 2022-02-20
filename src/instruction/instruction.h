#pragma once

#include <format>
#include <iostream>
#include <span>
#include <string>
#include <vector>

#include "opcodes.h"
#include "util/strings.h"


/**
 * @struct instruction
 * @brief  Represents a CHIP-8 instruction, decomposed into the opcode and individual arguments.
 */
struct instruction {
public:
    constexpr instruction(uint16_t op) {
        x      = (op & 0x0F00) >> 8;
        y      = (op & 0x00F0) >> 4;
        n      =  op & 0x000F;
        nn     =  op & 0x00FF;
        nnn    =  op & 0x0FFF;
        opcode = to_opcode(op);
    }

    constexpr instruction(uint8_t high, uint8_t low) :
        instruction((static_cast<uint16_t>(high) << 8) | low) {
    }

    constexpr instruction(Opcodes op, uint16_t data) :
        instruction(static_cast<uint16_t>(op) | data) {
    }

    // Get the packed representation of the instruction
    [[nodiscard]]
    constexpr explicit operator uint16_t() const noexcept {
        return static_cast<uint16_t>(opcode) | nnn | nn | n | (x << 8) | (y << 4);
    }

    // The opcode without arguments
    Opcodes opcode = Opcodes::invalid;

    // The arguments of the instruction
    uint16_t nnn : 12 = 0;
    uint8_t  nn       = 0;
    uint8_t  n : 4    = 0;
    uint8_t  x : 4    = 0;
    uint8_t  y : 4    = 0;
};


struct compile_result {
    std::span<const std::string> source;
    std::vector<uint16_t> program_data;
    std::vector<size_t> failures; ///line numbers in the source that couldn't be parsed
};


struct decompile_result {
    std::span<const uint8_t> source;
    std::vector<std::string> program;
    std::vector<size_t> failures; ///indices of 16-bit values in the source that couldn't be parsed
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

    // Remove the brackets in the shr/shl instruction
    if (instr.opcode == Opcodes::shl_vx or instr.opcode == Opcodes::shr_vx) {
        replace(op, "{", "");
        replace(op, "}", "");
    }

    return op;
}

[[nodiscard]]
auto to_instruction(std::string_view str) -> std::optional<instruction>;

[[nodiscard]]
auto compile_program(std::span<const std::string> lines) -> compile_result;

[[nodiscard]]
auto decompile_program(std::span<const uint8_t> data) -> decompile_result;
