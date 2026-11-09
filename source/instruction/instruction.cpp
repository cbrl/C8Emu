#include "instruction.h"

#include <format>


auto decompile_program(std::span<const uint8_t> data) -> decompile_result {
    auto result = decompile_result{};
    result.source = data;

    if (data.empty()) {
        return result;
    }

    for (size_t i = 0; i < (data.size() - 1); i += 2) {
        const auto instr_data = static_cast<uint16_t>((static_cast<uint16_t>(data[i]) << 8) | data[i + 1]);
        const auto instr = instruction{instr_data};

        // If this is a valid instruction, then convert it to a string. Otherwise, output the raw
        // byte value as a string instead. The constants are written as 16-bit values as well since
        // instructions are aligned to 16-bit boundaries.
        if (instr.opcode != Opcodes::invalid) {
            result.program.push_back(to_string(instr));
        }
        else {
            result.program.push_back(std::format("0x{:04X}", instr_data));
        }
    }

    // If there's an odd byte at the end, then write that out too. Put it in the high byte,
    // because the compiler writes the high byte first. Then the byte keeps its address.
    if ((data.size() % 2) != 0) {
        result.program.push_back(std::format("0x{:04X}", static_cast<uint16_t>(data.back() << 8)));
    }

    return result;
}
