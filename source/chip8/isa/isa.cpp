#include "isa.h"

#include "chip8/isa/decode.h"

#include <bit>

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


// Skip the next instruction if the condition is true. XO-CHIP skips all 4 bytes of F000 NNNN.
auto skip_if(MachineState& s, Variant variant, bool condition) noexcept -> void {
    if (!condition) {
        advance(s);
        return;
    }

    const uint16_t next = s.read_word(static_cast<uint32_t>(s.pc) + 2);
    const bool long_next = has_xochip(variant) && (next == 0xF000);
    advance(s, long_next ? 6 : 4);
}


// The planes that draw, clear, and scroll. Only XO-CHIP can select them.
auto selected_planes(const MachineState& s, Variant variant) noexcept -> uint8_t {
    return has_xochip(variant) ? s.planes : Display::all_planes;
}


auto apply_load_store_quirk(MachineState& s, const Quirks& q, uint8_t x) noexcept -> void {
    switch (q.load_store) {
        case LoadStoreQuirk::increment_x_plus_1: s.i = static_cast<uint16_t>(s.i + x + 1); break;
        case LoadStoreQuirk::increment_x:        s.i = static_cast<uint16_t>(s.i + x);     break;
        case LoadStoreQuirk::unchanged:          break;
    }
}


/**
 * Draw a sprite to each selected plane. A sprite is N rows of 8 pixels, or
 * 16 rows of 16 pixels for DXY0 on SUPER-CHIP and XO-CHIP. With two planes,
 * the data of plane 2 follows the data of plane 1.
 */
auto draw_sprite(MachineState& s, const Quirks& q, Variant variant, uint8_t x, uint8_t y, uint8_t n) noexcept -> void {
    auto& display = s.display;
    const size_t width  = display.width();
    const size_t height = display.height();

    const bool wide = (n == 0) && has_schip(variant);
    const size_t rows    = wide ? size_t{16} : size_t{n};
    const size_t columns = wide ? 16 : 8;
    const size_t bytes_per_row = wide ? 2 : 1;

    // The start position always wraps. Pixels past the edge clip or wrap per the quirk.
    const size_t x0 = s.v[x] % width;
    const size_t y0 = s.v[y] % height;

    // The other variants always draw to plane 1
    const uint8_t planes = has_xochip(variant) ? s.planes : uint8_t{1};

    bool collision = false;
    uint32_t address = s.i;

    for (uint8_t plane = 1; plane <= Display::all_planes; plane = static_cast<uint8_t>(plane << 1)) {
        if ((planes & plane) == 0) {
            continue;
        }

        for (size_t row = 0; row < rows; ++row) {
            size_t py = y0 + row;
            if (py >= height) {
                if (!q.wrap_sprites) {
                    break;
                }
                py %= height;
            }

            const uint32_t row_address = address + static_cast<uint32_t>(row * bytes_per_row);
            const uint16_t bits = wide
                ? static_cast<uint16_t>((s.read(row_address) << 8) | s.read(row_address + 1))
                : static_cast<uint16_t>(s.read(row_address) << 8);

            for (size_t col = 0; col < columns; ++col) {
                if ((bits & (0x8000u >> col)) == 0) {
                    continue;
                }

                size_t px = x0 + col;
                if (px >= width) {
                    if (!q.wrap_sprites) {
                        break;
                    }
                    px %= width;
                }

                if (display.toggle(px, py, plane)) {
                    collision = true;
                }
            }
        }

        address += static_cast<uint32_t>(rows * bytes_per_row);
    }

    // Write VF last, so that VF as a coordinate register reads its old value
    s.v[0xF] = static_cast<uint8_t>(collision);
}


// FX0A. The PC stays at the instruction until the wait completes. Keys held
// when the wait starts are ignored until they are released.
auto wait_for_key(MachineState& s, const Quirks& q, uint8_t x) noexcept -> ExecResult {
    auto& wait = s.key_wait;

    // Start a new wait, unless this instruction already waits
    if ((wait.phase == KeyWaitPhase::idle) || (wait.pc != s.pc) || (wait.reg != x)) {
        wait = {.phase = KeyWaitPhase::wait_press, .reg = x, .pc = s.pc, .ignored = s.keys};
    }

    // A released key is no longer ignored
    wait.ignored = static_cast<uint16_t>(wait.ignored & s.keys);

    if (wait.phase == KeyWaitPhase::wait_press) {
        const auto pressed = static_cast<uint16_t>(s.keys & ~wait.ignored);
        if (pressed == 0) {
            return {.effect = Effect::waiting};
        }

        // The lowest key wins if several keys are pressed at once
        wait.key = static_cast<uint8_t>(std::countr_zero(pressed));

        if (q.key_wait_release) {
            wait.phase = KeyWaitPhase::wait_release;
            return {.effect = Effect::waiting};
        }
    }
    else if (s.key_down(wait.key)) {
        return {.effect = Effect::waiting};
    }

    s.v[x] = wait.key;
    wait = {};
    advance(s);
    return {};
}


// Switch between lores (64x32) and hires (128x64). The switch clears the display.
auto set_resolution(MachineState& s, bool hires) noexcept -> void {
    if (hires) {
        s.display.resize(Display::max_width, Display::max_height);
    }
    else {
        s.display.resize(64, 32);
    }
}


auto execute_opcode(MachineState& s, const Quirks& q, Variant variant, uint16_t opcode) noexcept -> ExecResult {
    const auto x   = static_cast<uint8_t>((opcode >> 8) & 0xF);
    const auto y   = static_cast<uint8_t>((opcode >> 4) & 0xF);
    const auto n   = static_cast<uint8_t>(opcode & 0xF);
    const auto nn  = static_cast<uint8_t>(opcode & 0xFF);
    const auto nnn = static_cast<uint16_t>(opcode & 0xFFF);

    auto& v = s.v;

    // Hires CHIP-8 programs start with a jump to the interpreter extension at
    // 0x260. The extension is not emulated, so the jump goes to the program
    // after it at 0x2C0. The reset already selected the 64x64 display.
    if ((variant == Variant::chip8_hires) && (opcode == hires_entry_jump) && (s.pc == MachineState::program_start)) {
        s.pc = hires_program_start;
        return {};
    }

    switch (decode(opcode, variant)) {
        //--------------------------------------------------------------------------------
        // 0x0---
        //--------------------------------------------------------------------------------
        case Op::cls: // 00E0: clear the display (the selected planes on XO-CHIP)
            s.display.clear(selected_planes(s, variant));
            advance(s);
            return {};

        case Op::ret: // 00EE: return from a subroutine
            if (s.sp == 0 || s.sp > MachineState::stack_depth) {
                return {.fault = Fault::stack_underflow};
            }
            --s.sp;
            s.pc = s.stack[s.sp];
            return {};

        case Op::sys: // 0NNN: machine code routine. Modern interpreters ignore it.
            advance(s);
            return {};

        case Op::scd: // 00CN: scroll down N pixels
            s.display.scroll_down(n, selected_planes(s, variant));
            advance(s);
            return {};

        case Op::scu: // 00DN: scroll up N pixels
            s.display.scroll_up(n, selected_planes(s, variant));
            advance(s);
            return {};

        case Op::scr: // 00FB: scroll right 4 pixels
            s.display.scroll_right(4, selected_planes(s, variant));
            advance(s);
            return {};

        case Op::scl: // 00FC: scroll left 4 pixels
            s.display.scroll_left(4, selected_planes(s, variant));
            advance(s);
            return {};

        case Op::exit: // 00FD: stop. The PC stays here, so each later cycle exits again.
            s.exited = true;
            return {.effect = Effect::exited};

        case Op::low: // 00FE: lores mode
            set_resolution(s, false);
            advance(s);
            return {};

        case Op::high: // 00FF: hires mode
            set_resolution(s, true);
            advance(s);
            return {};

        //--------------------------------------------------------------------------------
        // Jumps and calls
        //--------------------------------------------------------------------------------
        case Op::jmp: // 1NNN: jump to NNN
            s.pc = nnn;
            return {};

        case Op::call: // 2NNN: call the subroutine at NNN
            if (s.sp >= MachineState::stack_depth) {
                return {.fault = Fault::stack_overflow};
            }
            s.stack[s.sp] = static_cast<uint16_t>(s.pc + 2);
            ++s.sp;
            s.pc = nnn;
            return {};

        case Op::jmp_v0: // BNNN: jump to NNN + V0, or XNN + VX with the jump quirk
            s.pc = static_cast<uint16_t>(nnn + (q.jump_uses_vx ? v[x] : v[0]));
            return {};

        //--------------------------------------------------------------------------------
        // Skips
        //--------------------------------------------------------------------------------
        case Op::se_vx_nn:  skip_if(s, variant, v[x] == nn);   return {}; // 3XNN
        case Op::sne_vx_nn: skip_if(s, variant, v[x] != nn);   return {}; // 4XNN
        case Op::se_vx_vy:  skip_if(s, variant, v[x] == v[y]); return {}; // 5XY0
        case Op::sne_vx_vy: skip_if(s, variant, v[x] != v[y]); return {}; // 9XY0

        case Op::skp:  skip_if(s, variant, s.key_down(v[x]));  return {}; // EX9E: skip if key VX is held
        case Op::sknp: skip_if(s, variant, !s.key_down(v[x])); return {}; // EXA1: skip if key VX is not held

        //--------------------------------------------------------------------------------
        // Loads and arithmetic
        //--------------------------------------------------------------------------------
        case Op::mov_vx_nn: // 6XNN: VX = NN
            v[x] = nn;
            advance(s);
            return {};

        case Op::add_vx_nn: // 7XNN: VX += NN, VF does not change
            v[x] = static_cast<uint8_t>(v[x] + nn);
            advance(s);
            return {};

        case Op::mov_vx_vy: // 8XY0: VX = VY
            v[x] = v[y];
            advance(s);
            return {};

        case Op::or_vx_vy: // 8XY1: VX |= VY
            v[x] = static_cast<uint8_t>(v[x] | v[y]);
            if (q.vf_reset) {
                v[0xF] = 0;
            }
            advance(s);
            return {};

        case Op::and_vx_vy: // 8XY2: VX &= VY
            v[x] = static_cast<uint8_t>(v[x] & v[y]);
            if (q.vf_reset) {
                v[0xF] = 0;
            }
            advance(s);
            return {};

        case Op::xor_vx_vy: // 8XY3: VX ^= VY
            v[x] = static_cast<uint8_t>(v[x] ^ v[y]);
            if (q.vf_reset) {
                v[0xF] = 0;
            }
            advance(s);
            return {};

        case Op::add_vx_vy: { // 8XY4: VX += VY, VF = carry
            const unsigned sum = static_cast<unsigned>(v[x]) + v[y];
            v[x]   = static_cast<uint8_t>(sum);
            v[0xF] = static_cast<uint8_t>(sum > 0xFF);
            advance(s);
            return {};
        }

        case Op::sub_vx_vy: { // 8XY5: VX -= VY, VF = NOT borrow
            const uint8_t a = v[x];
            const uint8_t b = v[y];
            v[x]   = static_cast<uint8_t>(a - b);
            v[0xF] = static_cast<uint8_t>(a >= b);
            advance(s);
            return {};
        }

        case Op::subn_vx_vy: { // 8XY7: VX = VY - VX, VF = NOT borrow
            const uint8_t a = v[x];
            const uint8_t b = v[y];
            v[x]   = static_cast<uint8_t>(b - a);
            v[0xF] = static_cast<uint8_t>(b >= a);
            advance(s);
            return {};
        }

        case Op::shr: { // 8XY6: VX = VY >> 1 (or VX >> 1), VF = the bit shifted out
            const uint8_t source = q.shift_uses_vy ? v[y] : v[x];
            v[x]   = static_cast<uint8_t>(source >> 1);
            v[0xF] = static_cast<uint8_t>(source & 1);
            advance(s);
            return {};
        }

        case Op::shl: { // 8XYE: VX = VY << 1 (or VX << 1), VF = the bit shifted out
            const uint8_t source = q.shift_uses_vy ? v[y] : v[x];
            v[x]   = static_cast<uint8_t>(source << 1);
            v[0xF] = static_cast<uint8_t>(source >> 7);
            advance(s);
            return {};
        }

        case Op::rnd: // CXNN: VX = random & NN
            v[x] = static_cast<uint8_t>(next_random(s.rng) & nn);
            advance(s);
            return {};

        //--------------------------------------------------------------------------------
        // Display
        //--------------------------------------------------------------------------------
        case Op::drw: // DXYN: draw N sprite rows from I at (VX, VY), VF = collision
            draw_sprite(s, q, variant, x, y, n);
            advance(s);
            return {.effect = Effect::drew};

        case Op::plane: // FN01: select the planes for drawing, clearing, and scrolling
            s.planes = static_cast<uint8_t>(x & Display::all_planes);
            advance(s);
            return {};

        //--------------------------------------------------------------------------------
        // Timers, keys, and sound
        //--------------------------------------------------------------------------------
        case Op::gdly: // FX07: VX = delay timer
            v[x] = s.delay_timer;
            advance(s);
            return {};

        case Op::key: // FX0A: wait for a key, then VX = key
            return wait_for_key(s, q, x);

        case Op::sdly: // FX15: delay timer = VX
            s.delay_timer = v[x];
            advance(s);
            return {};

        case Op::ssnd: // FX18: sound timer = VX
            s.sound_timer = v[x];
            advance(s);
            return {};

        case Op::audio: // F002: load the audio pattern from I
            for (uint32_t b = 0; b < s.audio_pattern.size(); ++b) {
                s.audio_pattern[b] = s.read(s.i + b);
            }
            s.audio_pattern_loaded = true;
            advance(s);
            return {};

        case Op::pitch: // FX3A: pitch = VX
            s.pitch = v[x];
            advance(s);
            return {};

        //--------------------------------------------------------------------------------
        // Memory and the I register
        //--------------------------------------------------------------------------------
        case Op::mov_i_nnn: // ANNN: I = NNN
            s.i = nnn;
            advance(s);
            return {};

        case Op::mov_i_long: // F000 NNNN: I = NNNN
            if ((static_cast<size_t>(s.pc) + 3) >= s.memory.size()) {
                return {.fault = Fault::pc_out_of_range};
            }
            s.i = s.read_word(static_cast<uint32_t>(s.pc) + 2);
            advance(s, 4);
            return {};

        case Op::add_i_vx: // FX1E: I += VX, VF does not change
            s.i = static_cast<uint16_t>(s.i + v[x]);
            advance(s);
            return {};

        case Op::font: // FX29: I = address of the font sprite for the low nibble of VX
            s.i = static_cast<uint16_t>(MachineState::font_address + ((v[x] & 0xF) * 5));
            advance(s);
            return {};

        case Op::hfont: // FX30: I = address of the big font sprite for the low nibble of VX
            s.i = static_cast<uint16_t>(MachineState::big_font_address + ((v[x] & 0xF) * 10));
            advance(s);
            return {};

        case Op::bcd: { // FX33: write the decimal digits of VX at I, I+1, I+2
            const uint8_t value = v[x];
            s.write(s.i,      static_cast<uint8_t>(value / 100));
            s.write(s.i + 1u, static_cast<uint8_t>((value / 10) % 10));
            s.write(s.i + 2u, static_cast<uint8_t>(value % 10));
            advance(s);
            return {};
        }

        case Op::str: // FX55: write V0..VX to memory at I
            for (uint8_t r = 0; r <= x; ++r) {
                s.write(static_cast<uint32_t>(s.i) + r, v[r]);
            }
            apply_load_store_quirk(s, q, x);
            advance(s);
            return {};

        case Op::ld: // FX65: read V0..VX from memory at I
            for (uint8_t r = 0; r <= x; ++r) {
                v[r] = s.read(static_cast<uint32_t>(s.i) + r);
            }
            apply_load_store_quirk(s, q, x);
            advance(s);
            return {};

        case Op::str_range: { // 5XY2: write VX..VY at I. X > Y writes in reverse order. I does not change.
            const int step = (x <= y) ? 1 : -1;
            uint32_t offset = 0;
            for (int r = x;; r += step, ++offset) {
                s.write(s.i + offset, v[static_cast<size_t>(r)]);
                if (r == y) {
                    break;
                }
            }
            advance(s);
            return {};
        }

        case Op::ld_range: { // 5XY3: read VX..VY from I. X > Y reads in reverse order. I does not change.
            const int step = (x <= y) ? 1 : -1;
            uint32_t offset = 0;
            for (int r = x;; r += step, ++offset) {
                v[static_cast<size_t>(r)] = s.read(s.i + offset);
                if (r == y) {
                    break;
                }
            }
            advance(s);
            return {};
        }

        case Op::sflags: // FX75: flags[0..X] = V0..VX
            for (uint8_t r = 0; r <= x; ++r) {
                s.flags[r] = v[r];
            }
            advance(s);
            return {};

        case Op::lflags: // FX85: V0..VX = flags[0..X]
            for (uint8_t r = 0; r <= x; ++r) {
                v[r] = s.flags[r];
            }
            advance(s);
            return {};

        case Op::invalid:
            break;
    }

    return {.fault = Fault::illegal_instruction};
}

} // namespace


auto execute(MachineState& s, const Quirks& q, Variant variant, uint16_t opcode) noexcept -> ExecResult {
    const auto result = execute_opcode(s, q, variant, opcode);

    // Another instruction ran, so an FX0A wait is over. This happens when the
    // user moves the PC or changes the FX0A word during the wait.
    if ((result.fault == Fault::none) && (s.key_wait.phase != KeyWaitPhase::idle) && (decode(opcode, variant) != Op::key)) {
        s.key_wait = {};
    }

    return result;
}

} // namespace c8::isa
