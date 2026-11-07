#include "isa.h"

#include "instruction/opcodes.h"

namespace c8::isa {
namespace {

auto next_random(uint32_t& state) noexcept -> uint8_t {
    // xorshift32. A zero state stays zero, so replace it.
    if (state == 0) {
        state = 1;
    }
    state ^= state << 13;
    state ^= state >> 17;
    state ^= state << 5;
    return static_cast<uint8_t>(state >> 24);
}


auto advance(MachineState& s, int count = 2) noexcept -> void {
    s.pc = static_cast<uint16_t>(s.pc + count);
}


auto skip_if(MachineState& s, bool condition) noexcept -> void {
    advance(s, condition ? 4 : 2);
}


auto apply_load_store_quirk(MachineState& s, const Quirks& q, uint8_t x) noexcept -> void {
    switch (q.load_store) {
        case LoadStoreQuirk::increment_x_plus_1: s.i = static_cast<uint16_t>(s.i + x + 1); break;
        case LoadStoreQuirk::increment_x:        s.i = static_cast<uint16_t>(s.i + x);     break;
        case LoadStoreQuirk::unchanged:          break;
    }
}


auto draw_sprite(MachineState& s, const Quirks& q, uint8_t x, uint8_t y, uint8_t rows) noexcept -> void {
    auto& display = s.display;
    const size_t width  = display.width();
    const size_t height = display.height();

    // The start position always wraps. Pixels past the edge clip or wrap per the quirk.
    const size_t x0 = s.v[x] % width;
    const size_t y0 = s.v[y] % height;

    bool collision = false;

    for (uint8_t row = 0; row < rows; ++row) {
        size_t py = y0 + row;
        if (py >= height) {
            if (!q.wrap_sprites) {
                break;
            }
            py %= height;
        }

        const uint8_t bits = s.read(static_cast<uint32_t>(s.i) + row);

        for (uint8_t col = 0; col < 8; ++col) {
            if ((bits & (0x80u >> col)) == 0) {
                continue;
            }

            size_t px = x0 + col;
            if (px >= width) {
                if (!q.wrap_sprites) {
                    break;
                }
                px %= width;
            }

            if (display.toggle(px, py)) {
                collision = true;
            }
        }
    }

    // Write VF last, so that VF as a coordinate register reads its old value
    s.v[0xF] = static_cast<uint8_t>(collision);
}

} // namespace


auto execute(MachineState& s, const Quirks& q, uint16_t opcode) noexcept -> ExecResult {
    const auto x   = static_cast<uint8_t>((opcode >> 8) & 0xF);
    const auto y   = static_cast<uint8_t>((opcode >> 4) & 0xF);
    const auto n   = static_cast<uint8_t>(opcode & 0xF);
    const auto nn  = static_cast<uint8_t>(opcode & 0xFF);
    const auto nnn = static_cast<uint16_t>(opcode & 0xFFF);

    auto& v = s.v;

    switch (to_opcode(opcode)) {
        //--------------------------------------------------------------------------------
        // 0x0---
        //--------------------------------------------------------------------------------
        case Opcodes::cls: // 00E0: clear the display
            s.display.clear();
            advance(s);
            return {};

        case Opcodes::ret: // 00EE: return from a subroutine
            if (s.sp == 0 || s.sp > MachineState::stack_depth) {
                return {.fault = Fault::stack_underflow};
            }
            --s.sp;
            s.pc = s.stack[s.sp];
            return {};

        case Opcodes::sys_nnn: // 0NNN: machine code routine. Modern interpreters ignore it.
            advance(s);
            return {};

        //--------------------------------------------------------------------------------
        // Jumps and calls
        //--------------------------------------------------------------------------------
        case Opcodes::jmp_nnn: // 1NNN: jump to NNN
            s.pc = nnn;
            return {};

        case Opcodes::call_nnn: // 2NNN: call the subroutine at NNN
            if (s.sp >= MachineState::stack_depth) {
                return {.fault = Fault::stack_overflow};
            }
            s.stack[s.sp] = static_cast<uint16_t>(s.pc + 2);
            ++s.sp;
            s.pc = nnn;
            return {};

        case Opcodes::jmp_v0_nnn: // BNNN: jump to NNN + V0, or XNN + VX with the jump quirk
            s.pc = static_cast<uint16_t>(nnn + (q.jump_uses_vx ? v[x] : v[0]));
            return {};

        //--------------------------------------------------------------------------------
        // Skips
        //--------------------------------------------------------------------------------
        case Opcodes::se_vx_nn:  skip_if(s, v[x] == nn);   return {}; // 3XNN
        case Opcodes::sne_vx_nn: skip_if(s, v[x] != nn);   return {}; // 4XNN
        case Opcodes::se_vx_vy:  skip_if(s, v[x] == v[y]); return {}; // 5XY0
        case Opcodes::sne_vx_vy: skip_if(s, v[x] != v[y]); return {}; // 9XY0

        case Opcodes::skp_vx:  skip_if(s, s.key_down(v[x]));  return {}; // EX9E: skip if key VX is held
        case Opcodes::sknp_vx: skip_if(s, !s.key_down(v[x])); return {}; // EXA1: skip if key VX is not held

        //--------------------------------------------------------------------------------
        // Loads and arithmetic
        //--------------------------------------------------------------------------------
        case Opcodes::mov_vx_nn: // 6XNN: VX = NN
            v[x] = nn;
            advance(s);
            return {};

        case Opcodes::add_vx_nn: // 7XNN: VX += NN, VF does not change
            v[x] = static_cast<uint8_t>(v[x] + nn);
            advance(s);
            return {};

        case Opcodes::mov_vx_vy: // 8XY0: VX = VY
            v[x] = v[y];
            advance(s);
            return {};

        case Opcodes::or_vx_vy: // 8XY1: VX |= VY
            v[x] = static_cast<uint8_t>(v[x] | v[y]);
            if (q.vf_reset) {
                v[0xF] = 0;
            }
            advance(s);
            return {};

        case Opcodes::and_vx_vy: // 8XY2: VX &= VY
            v[x] = static_cast<uint8_t>(v[x] & v[y]);
            if (q.vf_reset) {
                v[0xF] = 0;
            }
            advance(s);
            return {};

        case Opcodes::xor_vx_vy: // 8XY3: VX ^= VY
            v[x] = static_cast<uint8_t>(v[x] ^ v[y]);
            if (q.vf_reset) {
                v[0xF] = 0;
            }
            advance(s);
            return {};

        case Opcodes::add_vx_vy: { // 8XY4: VX += VY, VF = carry
            const unsigned sum = static_cast<unsigned>(v[x]) + v[y];
            v[x]   = static_cast<uint8_t>(sum);
            v[0xF] = static_cast<uint8_t>(sum > 0xFF);
            advance(s);
            return {};
        }

        case Opcodes::sub_vx_vy: { // 8XY5: VX -= VY, VF = NOT borrow
            const uint8_t a = v[x];
            const uint8_t b = v[y];
            v[x]   = static_cast<uint8_t>(a - b);
            v[0xF] = static_cast<uint8_t>(a >= b);
            advance(s);
            return {};
        }

        case Opcodes::subn_vx_vy: { // 8XY7: VX = VY - VX, VF = NOT borrow
            const uint8_t a = v[x];
            const uint8_t b = v[y];
            v[x]   = static_cast<uint8_t>(b - a);
            v[0xF] = static_cast<uint8_t>(b >= a);
            advance(s);
            return {};
        }

        case Opcodes::shr_vx: { // 8XY6: VX = VY >> 1 (or VX >> 1), VF = the bit shifted out
            const uint8_t source = q.shift_uses_vy ? v[y] : v[x];
            v[x]   = static_cast<uint8_t>(source >> 1);
            v[0xF] = static_cast<uint8_t>(source & 1);
            advance(s);
            return {};
        }

        case Opcodes::shl_vx: { // 8XYE: VX = VY << 1 (or VX << 1), VF = the bit shifted out
            const uint8_t source = q.shift_uses_vy ? v[y] : v[x];
            v[x]   = static_cast<uint8_t>(source << 1);
            v[0xF] = static_cast<uint8_t>(source >> 7);
            advance(s);
            return {};
        }

        case Opcodes::rnd_vx_nn: // CXNN: VX = random & NN
            v[x] = static_cast<uint8_t>(next_random(s.rng) & nn);
            advance(s);
            return {};

        //--------------------------------------------------------------------------------
        // Display
        //--------------------------------------------------------------------------------
        case Opcodes::drw_vx_vy_n: // DXYN: draw N sprite rows from I at (VX, VY), VF = collision
            draw_sprite(s, q, x, y, n);
            advance(s);
            return {.effect = Effect::drew};

        //--------------------------------------------------------------------------------
        // Timers and keys
        //--------------------------------------------------------------------------------
        case Opcodes::gdly_vx: // FX07: VX = delay timer
            v[x] = s.delay_timer;
            advance(s);
            return {};

        case Opcodes::key_vx: // FX0A: wait for a key. The caller stores the key and advances the PC.
            return {.effect = Effect::waiting};

        case Opcodes::sdly_vx: // FX15: delay timer = VX
            s.delay_timer = v[x];
            advance(s);
            return {};

        case Opcodes::ssnd_vx: // FX18: sound timer = VX
            s.sound_timer = v[x];
            advance(s);
            return {};

        //--------------------------------------------------------------------------------
        // Memory and the I register
        //--------------------------------------------------------------------------------
        case Opcodes::mov_i_nnn: // ANNN: I = NNN
            s.i = nnn;
            advance(s);
            return {};

        case Opcodes::add_i_vx: // FX1E: I += VX, VF does not change
            s.i = static_cast<uint16_t>(s.i + v[x]);
            advance(s);
            return {};

        case Opcodes::font_vx: // FX29: I = address of the font sprite for the low nibble of VX
            s.i = static_cast<uint16_t>(MachineState::font_address + ((v[x] & 0xF) * 5));
            advance(s);
            return {};

        case Opcodes::bcd_vx: { // FX33: write the decimal digits of VX at I, I+1, I+2
            const uint8_t value = v[x];
            s.write(s.i,     static_cast<uint8_t>(value / 100));
            s.write(s.i + 1u, static_cast<uint8_t>((value / 10) % 10));
            s.write(s.i + 2u, static_cast<uint8_t>(value % 10));
            advance(s);
            return {};
        }

        case Opcodes::str_v0_vx: // FX55: write V0..VX to memory at I
            for (uint8_t r = 0; r <= x; ++r) {
                s.write(static_cast<uint32_t>(s.i) + r, v[r]);
            }
            apply_load_store_quirk(s, q, x);
            advance(s);
            return {};

        case Opcodes::ld_v0_vx: // FX65: read V0..VX from memory at I
            for (uint8_t r = 0; r <= x; ++r) {
                v[r] = s.read(static_cast<uint32_t>(s.i) + r);
            }
            apply_load_store_quirk(s, q, x);
            advance(s);
            return {};

        case Opcodes::invalid:
            break;
    }

    return {.fault = Fault::illegal_instruction};
}

} // namespace c8::isa
