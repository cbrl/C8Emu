#include "opcodes.h"
#include <map>


std::string to_string(Opcodes op) {
    static const std::map<Opcodes, std::string> opcode_map = {
        {Opcodes::sys_nnn,     "sys nnn"},
        {Opcodes::cls,         "cls"},
        {Opcodes::ret,         "ret"},
        {Opcodes::jmp_nnn,     "jmp nnn"},
        {Opcodes::call_nnn,    "call nnn"},
        {Opcodes::se_vx_nn,    "se vx nn"},
        {Opcodes::sne_vx_nn,   "sne vx nn"},
        {Opcodes::se_vx_vy,    "se vx vy"},
        {Opcodes::mov_vx_nn,   "mov vx nn"},
        {Opcodes::add_vx_nn,   "add vx nn"},
        {Opcodes::mov_vx_vy,   "mov vx vy"},
        {Opcodes::or_vx_vy,    "or vx vy"},
        {Opcodes::and_vx_vy,   "and vx vy"},
        {Opcodes::xor_vx_vy,   "xor vx vy"},
        {Opcodes::add_vx_vy,   "add vx vy"},
        {Opcodes::sub_vx_vy,   "sub vx vy"},
        {Opcodes::shr_vx,      "shr vx"},
        {Opcodes::subn_vx_vy,  "subn vx vy"},
        {Opcodes::shl_vx,      "shl vx"},
        {Opcodes::sne_vx_vy,   "sne vx vy"},
        {Opcodes::mov_i_nnn,   "mov i nnn"},
        {Opcodes::jmp_v0_nnn,  "jmp v0 nnn"},
        {Opcodes::rnd_vx_nn,   "rnd vx nn"},
        {Opcodes::drw_vx_vy_n, "drw vx vy n"},
        {Opcodes::skp_vx,      "skp vx"},
        {Opcodes::sknp_vx,     "sknp vx"},
        {Opcodes::gdly_vx,     "gdly vx"},
        {Opcodes::key_vx,      "key vx"},
        {Opcodes::sdly_vx,     "sdly vx"},
        {Opcodes::ssnd_vx,     "ssnd vx"},
        {Opcodes::add_i_vx,    "add i vx"},
        {Opcodes::font_vx,     "font vx"},
        {Opcodes::bcd_vx,      "bcd vx"},
        {Opcodes::str_v0_vx,   "str v0 vx"},
        {Opcodes::ld_v0_vx,    "ld v0 vx"},
        {Opcodes::invalid,     "invalid"}
    };

    return opcode_map.at(op);
}


Opcodes to_opcode(std::string_view op) {
    static const std::map<std::string, Opcodes, std::less<>> opcode_map = {
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
        {"shr vx",      Opcodes::shr_vx},
        {"subn vx vy",  Opcodes::subn_vx_vy},
        {"shl vx",      Opcodes::shl_vx},
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
        {"str v0 vx",   Opcodes::str_v0_vx},
        {"ld v0 vx",    Opcodes::ld_v0_vx},
    };

    if (const auto it = opcode_map.find(op); it != opcode_map.end()) {
        return it->second;
    }
    return Opcodes::invalid;
}


Opcodes to_opcode(uint16_t instruction) {
    const uint16_t msb = instruction & 0xF000;

    switch (msb) {
        case 0x0000: {
            if ((instruction & 0x0F00) == 0) {
                const uint16_t lsb = instruction & 0x00FF;
                switch (lsb) {
                    case 0x00E0:
                    case 0x00EE:
                        return static_cast<Opcodes>(msb + lsb);
                }
            }
            return Opcodes::sys_nnn;
        }

        case 0x8000: {
            const uint16_t lsb = instruction & 0x000F;
            if ((lsb <= 0x0007) || (lsb == 0x000E)) {
                return static_cast<Opcodes>(msb + lsb); 
            }
            return Opcodes::invalid;
        }

        case 0xE000: {
            const uint16_t lsb = instruction & 0x00FF;
            if ((lsb == 0x009E) || (lsb == 0x00A1)) {
                return static_cast<Opcodes>(msb + lsb);
            }
            return Opcodes::invalid;
        }

        case 0xF000: {
            const uint16_t lsb = instruction & 0x00FF;
            switch (lsb) {
                case 0x0007:
                case 0x000A:
                case 0x0015:
                case 0x0018:
                case 0x001E:
                case 0x0029:
                case 0x0033:
                case 0x0055:
                case 0x0065:
                    return static_cast<Opcodes>(msb + lsb); 
            }
            return Opcodes::invalid;
        }

        default:
            return static_cast<Opcodes>(msb);
    }
}
