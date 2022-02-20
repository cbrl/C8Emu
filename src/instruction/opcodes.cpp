#include "opcodes.h"
#include <unordered_map>


auto to_string(Opcodes op) -> std::string {
    switch (op) {
        case Opcodes::sys_nnn:     return "sys nnn";
        case Opcodes::cls:         return "cls";
        case Opcodes::ret:         return "ret";
        case Opcodes::jmp_nnn:     return "jmp nnn";
        case Opcodes::call_nnn:    return "call nnn";
        case Opcodes::se_vx_nn:    return "se vx nn";
        case Opcodes::sne_vx_nn:   return "sne vx nn";
        case Opcodes::se_vx_vy:    return "se vx vy";
        case Opcodes::mov_vx_nn:   return "mov vx nn";
        case Opcodes::add_vx_nn:   return "add vx nn";
        case Opcodes::mov_vx_vy:   return "mov vx vy";
        case Opcodes::or_vx_vy:    return "or vx vy";
        case Opcodes::and_vx_vy:   return "and vx vy";
        case Opcodes::xor_vx_vy:   return "xor vx vy";
        case Opcodes::add_vx_vy:   return "add vx vy";
        case Opcodes::sub_vx_vy:   return "sub vx vy";
        case Opcodes::shr_vx:      return "shr vx {vy}";
        case Opcodes::subn_vx_vy:  return "subn vx vy";
        case Opcodes::shl_vx:      return "shl vx {vy}";
        case Opcodes::sne_vx_vy:   return "sne vx vy";
        case Opcodes::mov_i_nnn:   return "mov i nnn";
        case Opcodes::jmp_v0_nnn:  return "jmp v0 nnn";
        case Opcodes::rnd_vx_nn:   return "rnd vx nn";
        case Opcodes::drw_vx_vy_n: return "drw vx vy n";
        case Opcodes::skp_vx:      return "skp vx";
        case Opcodes::sknp_vx:     return "sknp vx";
        case Opcodes::gdly_vx:     return "gdly vx";
        case Opcodes::key_vx:      return "key vx";
        case Opcodes::sdly_vx:     return "sdly vx";
        case Opcodes::ssnd_vx:     return "ssnd vx";
        case Opcodes::add_i_vx:    return "add i vx";
        case Opcodes::font_vx:     return "font vx";
        case Opcodes::bcd_vx:      return "bcd vx";
        case Opcodes::str_v0_vx:   return "str vx";
        case Opcodes::ld_v0_vx:    return "ld vx";
        case Opcodes::invalid:     return "invalid";
        default:                   return "invalid";
    }
}


auto to_opcode(std::string_view op) -> Opcodes {
    struct string_hash {
        using is_transparent = void;

        [[nodiscard]]
        size_t operator()(const char* txt) const {
            return std::hash<std::string_view>{}(txt);
        }

        [[nodiscard]]
        size_t operator()(std::string_view txt) const {
            return std::hash<std::string_view>{}(txt);
        }

        [[nodiscard]]
        size_t operator()(const std::string& txt) const {
            return std::hash<std::string>{}(txt);
        }
    };

    static const auto opcode_map = std::unordered_map<std::string, Opcodes, string_hash, std::equal_to<>>{
        {"sys nnn",     Opcodes::sys_nnn},
        {"cls",         Opcodes::cls},
        {"ret",         Opcodes::ret},
        {"jmp nnn",     Opcodes::jmp_nnn},
        {"call nnn",    Opcodes::call_nnn},
        {"se vx nn",    Opcodes::se_vx_nn},
        {"sne vx nn",   Opcodes::sne_vx_nn},
        {"se vx vy",    Opcodes::se_vx_vy},
        {"mov vx nn",   Opcodes::mov_vx_nn},
        {"add vx nn",   Opcodes::add_vx_nn},
        {"mov vx vy",   Opcodes::mov_vx_vy},
        {"or vx vy",    Opcodes::or_vx_vy},
        {"and vx vy",   Opcodes::and_vx_vy},
        {"xor vx vy",   Opcodes::xor_vx_vy},
        {"add vx vy",   Opcodes::add_vx_vy},
        {"sub vx vy",   Opcodes::sub_vx_vy},
        {"shr vx {vy}", Opcodes::shr_vx},
        {"subn vx vy",  Opcodes::subn_vx_vy},
        {"shl vx {vy}", Opcodes::shl_vx},
        {"sne vx vy",   Opcodes::sne_vx_vy},
        {"mov i nnn",   Opcodes::mov_i_nnn},
        {"jmp v0 nnn",  Opcodes::jmp_v0_nnn},
        {"rnd vx nn",   Opcodes::rnd_vx_nn},
        {"drw vx vy n", Opcodes::drw_vx_vy_n},
        {"skp vx",      Opcodes::skp_vx},
        {"sknp vx",     Opcodes::sknp_vx},
        {"gdly vx",     Opcodes::gdly_vx},
        {"key vx",      Opcodes::key_vx},
        {"sdly vx",     Opcodes::sdly_vx},
        {"ssnd vx",     Opcodes::ssnd_vx},
        {"add i vx",    Opcodes::add_i_vx},
        {"font vx",     Opcodes::font_vx},
        {"bcd vx",      Opcodes::bcd_vx},
        {"str vx",      Opcodes::str_v0_vx},
        {"ld vx",       Opcodes::ld_v0_vx},
    };

    if (const auto it = opcode_map.find(op); it != opcode_map.end()) {
        return it->second;
    }
    return Opcodes::invalid;
}
