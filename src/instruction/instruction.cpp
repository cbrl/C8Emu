#include "instruction.h"

#include <cctype>
#include <format>
#include <ranges>
#include <string_view>


namespace {
static const auto rng_to_string = [](auto&& rng) { return std::string{rng.begin(), rng.end()}; };

[[nodiscard]]
auto format_instruction_str(std::string_view str) -> std::string {
    static const auto to_lower = std::views::transform([](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    return rng_to_string(str | to_lower);
}

[[nodiscard]]
auto split_instruction_parts(std::string_view str) -> std::vector<std::string> {
    const auto formatted = format_instruction_str(str);
    auto split = formatted | std::views::split(' ') | std::views::transform(rng_to_string);
    return std::vector<std::string>{std::ranges::begin(split), std::ranges::end(split)};
}
}


auto to_instruction(std::string_view str) -> std::optional<instruction> {
    if (str.empty()) {
        return std::nullopt;
    }

    // Convert the string to lowercase and split it by spaces
    const auto parts = split_instruction_parts(str);
    if (parts.empty()) {
        return std::nullopt;
    }

    static const auto get_vx = [](auto&& parts, size_t arg_idx) -> std::optional<uint16_t> {
        if (parts[arg_idx][0] != 'v') {
            return std::nullopt;
        }
        //if (auto vx = str_to<uint16_t>(parts[arg_idx].substr(1), 16)) { //remove the 'v' from the beginning
        //    return *vx << 8;
        //}
        //return std::nullopt;
        try {
            return static_cast<uint16_t>(std::strtoul(parts[arg_idx].substr(1).data(), nullptr, 16)) << 8;
        }
        catch (...) {
            return std::nullopt;
        }
    };
    static const auto get_vy = [](auto&& parts, size_t arg_idx) -> std::optional<uint16_t> {
        if (parts[arg_idx][0] != 'v') {
            return std::nullopt;
        }
        //if (auto vy = str_to<uint16_t>(parts[arg_idx].substr(1), 16)) {
        //    return *vy << 4;
        //}
        //return std::nullopt;
        try {
            return static_cast<uint16_t>(std::strtoul(parts[arg_idx].substr(1).data(), nullptr, 16)) << 4;
        }
        catch (...) {
            return std::nullopt;
        }
    };
    static const auto get_n = [](auto&& parts, size_t arg_idx) -> std::optional<uint16_t> {
        //return str_to<uint16_t>(parts[arg_idx], 16);
        try {
            return static_cast<uint16_t>(std::strtoul(parts[arg_idx].data(), nullptr, 16));
        }
        catch (...) {
            return std::nullopt;
        }
    };

    if (parts[0] == "sys" and parts.size() == 2) {
        if (auto nnn = get_n(parts, 1)) {
            return instruction(Opcodes::sys_nnn, *nnn);
        }
    }
    else if (parts[0] == "cls" and parts.size() == 1) {
        return instruction(Opcodes::cls, 0);
    }
    else if (parts[0] == "ret" and parts.size() == 1) {
        return instruction(Opcodes::ret, 0);
    }
    else if (parts[0] == "jmp") {
        if (parts.size() == 2) {
            if (auto nnn = get_n(parts, 1)) {
                return instruction(Opcodes::jmp_nnn, *nnn);
            }
        }
        else if (parts.size() == 3) {
            if (auto nnn = get_n(parts, 2)) {
                return instruction(Opcodes::jmp_v0_nnn, *nnn);
            }
        }
    }
    else if (parts[0] == "call" and parts.size() == 2) {
        if (auto nnn = get_n(parts, 1)) {
            return instruction(Opcodes::call_nnn, *nnn);
        }
    }
    else if (parts[0] == "se" and parts.size() == 3) {
        auto vx = get_vx(parts, 1);

        if (parts[2][0] == 'v') {
            if (auto vy = get_vy(parts, 2)) {
                return instruction(Opcodes::se_vx_vy, *vx | *vy);
            }
        }
        else {
            if (auto nn = get_n(parts, 2)) {
                return instruction(Opcodes::se_vx_nn, *vx | *nn);
            }
        }
    }
    else if (parts[0] == "sne" and parts.size() == 3) {
        auto vx = get_vx(parts, 1);

        if (parts[2][0] == 'v') {
            if (auto vy = get_vy(parts, 2)) {
                return instruction(Opcodes::sne_vx_vy, *vx | *vy);
            }
        }
        else {
            if (auto nn = get_n(parts, 2)) {
                return instruction(Opcodes::sne_vx_nn, *vx | *nn);
            }
        }
    }
    else if (parts[0] == "mov" and parts.size() == 3) {
        if (parts[1][0] == 'i') {
            if (auto nnn = get_n(parts, 2)) {
                return instruction(Opcodes::mov_i_nnn, *nnn);
            }
        }
        else {
            auto vx = get_vx(parts, 1);

            if (parts[2][0] == 'v') {
                if (auto vy = get_vy(parts, 2)) {
                    return instruction(Opcodes::mov_vx_vy, *vx | *vy);
                }
            }
            else {
                if (auto nnn = get_n(parts, 2)) {
                    return instruction(Opcodes::mov_vx_nn, *vx | *nnn);
                }
            }
        }
    }
    else if (parts[0] == "add") {
        if (parts[1][0] == 'i') {
            if (auto vx = get_vx(parts, 2)) {
                return instruction(Opcodes::add_i_vx, *vx);
            }
        }
        else {
            auto vx = get_vx(parts, 1);

            if (parts[2][0] == 'v') {
                if (auto vy = get_vy(parts, 2)) {
                    return instruction(Opcodes::add_vx_vy, *vx | *vy);
                }
            }
            else {
                if (auto nn = get_n(parts, 2)) {
                    return instruction(Opcodes::add_vx_nn, *vx | *nn);
                }
            }
        }
    }
    else if (parts[0] == "or" and parts.size() == 3) {
        auto vx = get_vx(parts, 1);
        auto vy = get_vy(parts, 2);

        if (vx and vy) {
            return instruction(Opcodes::or_vx_vy, *vx | *vy);
        }
    }
    else if (parts[0] == "and" and parts.size() == 3) {
        auto vx = get_vx(parts, 1);
        auto vy = get_vy(parts, 2);

        if (vx and vy) {
            return instruction(Opcodes::and_vx_vy, *vx | *vy);
        }
    }
    else if (parts[0] == "xor" and parts.size() == 3) {
        auto vx = get_vx(parts, 1);
        auto vy = get_vy(parts, 2);

        if (vx and vy) {
            return instruction(Opcodes::xor_vx_vy, *vx | *vy);
        }
    }
    else if (parts[0] == "sub" and parts.size() == 3) {
        auto vx = get_vx(parts, 1);
        auto vy = get_vy(parts, 2);

        if (vx and vy) {
            return instruction(Opcodes::sub_vx_vy, *vx | *vy);
        }
    }
    else if (parts[0] == "shr" and parts.size() >= 2) {
        if (auto vx = get_vx(parts, 1)) {
            if (parts.size() == 2) {
                // Set vy to vx for non-legacy shr instruction
                return instruction(Opcodes::shr_vx, *vx | (*vx >> 4));
            }
            else if (auto vy = get_vy(parts, 2)) {
                return instruction(Opcodes::shr_vx, *vx | *vy);
            }
        }
    }
    else if (parts[0] == "subn" and parts.size() == 3) {
        auto vx = get_vx(parts, 1);
        auto vy = get_vy(parts, 2);

        if (vx and vy) {
            return instruction(Opcodes::subn_vx_vy, *vx | *vy);
        }
    }
    else if (parts[0] == "shl" and parts.size() >= 2) {
        if (auto vx = get_vx(parts, 1)) {
            if (parts.size() == 2) {
                // Set vy to vx for non-legacy shl instruction
                return instruction(Opcodes::shl_vx, *vx | (*vx >> 4));
            }
            else if (auto vy = get_vy(parts, 2)) {
                return instruction(Opcodes::shl_vx, *vx | *vy);
            }
        }
    }
    else if (parts[0] == "rnd" and parts.size() == 3) {
        auto vx = get_vx(parts, 1);
        auto nn = get_n(parts, 2);

        if (vx and nn) {
            return instruction(Opcodes::rnd_vx_nn, *vx | *nn);
        }
    }
    else if (parts[0] == "drw" and parts.size() == 4) {
        auto vx = get_vx(parts, 1);
        auto vy = get_vy(parts, 2);
        auto n  = get_n(parts, 3);

        if (vx and vy and n) {
            return instruction(Opcodes::drw_vx_vy_n, *vx | *vy | *n);
        }
    }
    else if (parts[0] == "skp" and parts.size() == 2) {
        if (auto vx = get_vx(parts, 1)) {
            return instruction(Opcodes::skp_vx, *vx);
        }
    }
    else if (parts[0] == "sknp" and parts.size() == 2) {
        if (auto vx = get_vx(parts, 1)) {
            return instruction(Opcodes::sknp_vx, *vx);
        }
    }
    else if (parts[0] == "gdly" and parts.size() == 2) {
        if (auto vx = get_vx(parts, 1)) {
            return instruction(Opcodes::gdly_vx, *vx);
        }
    }
    else if (parts[0] == "key" and parts.size() == 2) {
        if (auto vx = get_vx(parts, 1)) {
            return instruction(Opcodes::key_vx, *vx);
        }
    }
    else if (parts[0] == "sdly" and parts.size() == 2) {
        if (auto vx = get_vx(parts, 1)) {
            return instruction(Opcodes::sdly_vx, *vx);
        }
    }
    else if (parts[0] == "ssnd" and parts.size() == 2) {
        if (auto vx = get_vx(parts, 1)) {
            return instruction(Opcodes::ssnd_vx, *vx);
        }
    }
    else if (parts[0] == "font" and parts.size() == 2) {
        if (auto vx = get_vx(parts, 1)) {
            return instruction(Opcodes::font_vx, *vx);
        }
    }
    else if (parts[0] == "bcd" and parts.size() == 2) {
        if (auto vx = get_vx(parts, 1)) {
            return instruction(Opcodes::bcd_vx, *vx);
        }
    }
    else if (parts[0] == "str" and parts.size() == 2) {
        if (auto vx = get_vx(parts, 1)) {
            return instruction(Opcodes::str_v0_vx, *vx);
        }
    }
    else if (parts[0] == "ld" and parts.size() == 2) {
        if (auto vx = get_vx(parts, 1)) {
            return instruction(Opcodes::ld_v0_vx, *vx);
        }
    }

    return std::nullopt;
}


[[nodiscard]]
auto compile_program(std::span<const std::string> lines) -> compile_result {
    auto result = compile_result{};
    result.source = lines;

    for (size_t i = 0; i < lines.size(); ++i) {
        auto& line = lines[i];

        if (auto instr = to_instruction(line)) {
            result.program_data.push_back(static_cast<uint16_t>(*instr));
        }
        else {
            const auto parts = split_instruction_parts(line);

            // Try to parse the line as a constant instead of an instruction
            if (parts.size() == 1) {
                try {
                    result.program_data.push_back(std::stoul(line, nullptr, 16));
                }
                catch (...) {
                    result.failures.push_back(i);
                }
            }
            else {
                // If all else fails, then insert empty data here and log an error.
                result.program_data.push_back(0);
                result.failures.push_back(i);
            }
        }
    }

    return result;
}


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

    // If there's an odd byte at the end, then write that out too.
    if ((data.size() % 2) == 0) {
        result.program.push_back(std::format("0x{:04X}", data.back()));
    }

    return result;
}
