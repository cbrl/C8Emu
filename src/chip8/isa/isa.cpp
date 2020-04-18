#include "isa.h"
#include "../chip8.h"

#include <iostream>
#include <random>
#include <sstream>


void ISA::execute_cycle(chip8& chip) {
	instruction instr{chip.memory[chip.pc], chip.memory[chip.pc+1]};
    opcode_map.at(instr.opcode)(chip, instr);
}


void ISA::increment_pc(chip8& chip) noexcept {
	chip.pc += 2;
}


void ISA::cls(chip8& chip, instruction instr) {

	// 0x00E0 - cls
	// Clear the display

	chip.display.clear();
	increment_pc(chip);
}


void ISA::ret(chip8& chip, instruction instr) {

	// 0x00EE - ret
	// Return from a subroutine

	// The interpreter sets the program counter to the address at the
	// top of the stack, then subtracts 1 from the stack pointer.

	chip.pc = chip.stack.top();
	chip.stack.pop();
	increment_pc(chip);
}


void ISA::sys_nnn(chip8& chip, instruction instr) {

	// 0x0nnn - sys addr
	// Jump to a machine code routine at nnn

	// This instr is only used on the old computers on which
	// Chip-8 was originally implemented. It is ignored by modern interpreters.

	increment_pc(chip);
}

void ISA::jmp_nnn(chip8& chip, instruction instr) {

	// 0x1nnn - jmp addr
	// Jump to location nnn

	// The interpreter sets the program counter to nnn.

	chip.pc = instr.nnn;
}


void ISA::call_nnn(chip8& chip, instruction instr) {

	// 0x2nnn - call addr
	// Call subroutine at nnn

	// The interpreter increments the stack pointer, then puts the
	// current pc on the top of the stack. The pc is then set to nnn.

	chip.stack.push(chip.pc);
	chip.pc = instr.nnn;
}


void ISA::se_vx_nn(chip8& chip, instruction instr) {

	// 0x3xnn - se vx, byte
	// Skip next instr if vx = nn

	// The interpreter compares register vx to nn, and if they are
	// equal, increments the program counter by 2.

	if (chip.v[instr.x] == instr.nn) {
		increment_pc(chip);
	}
	increment_pc(chip);
}


void ISA::sne_vx_nn(chip8& chip, instruction instr) {

	// 0x4xnn - sne vx, byte
	// Skip next instr if vx != nn

	// The interpreter compares register vx to nn, and if they are
	// not equal, increments the program counter by 2.

	if (chip.v[instr.x] != instr.nn) {
		increment_pc(chip);
	}
	increment_pc(chip);
}


void ISA::se_vx_vy(chip8& chip, instruction instr) {

	// 0x5xy0 - se vx, vy
	// Skip next instr if vx = vy

	// The interpreter compares register vx to register vy, and if they are
	// equal, increments the program counter by 2.

	if (chip.v[instr.x] == chip.v[instr.y]) {
		increment_pc(chip);
	}
	increment_pc(chip);
}


void ISA::mov_vx_nn(chip8& chip, instruction instr) {

	// 0x6xnn - mov vx, byte
	// Set vx = nn

	// The interpreter puts the value nn into register vx.

	chip.v[instr.x] = instr.nn;
	increment_pc(chip);
}


void ISA::add_vx_nn(chip8& chip, instruction instr) {

	// 0x7xnn - add vx, byte
	// Set vx = vx + nn

	// Adds the value nn to the value of register vx, then stores the result in vx.

	chip.v[instr.x] += instr.nn;
	increment_pc(chip);
}


void ISA::mov_vx_vy(chip8& chip, instruction instr) {

	// 8xy0 - mov vx, vy
	// Set vx = vy

	// Stores the value of register vy in register vx.

	chip.v[instr.x] = chip.v[instr.y];
	increment_pc(chip);
}


void ISA::or_vx_vy(chip8& chip, instruction instr) {

	// 0x8xy1 - or vx, vy
	// Set vx = vx | vy

	// Performs a bitwise OR on the values of vx and vy,
	// then stores the result in vx.

	chip.v[instr.x] = (chip.v[instr.x] | chip.v[instr.y]);
	increment_pc(chip);
}


void ISA::and_vx_vy(chip8& chip, instruction instr) {

	// 0x8xy2 - and vx, vy
	// Set vx = vx & vy

	// Performs a bitwise AND on the values of vx and vy,
	// then stores the result in vx.
	chip.v[instr.x] = (chip.v[instr.x] & chip.v[instr.y]);
	increment_pc(chip);
}


void ISA::xor_vx_vy(chip8& chip, instruction instr) {

	// 0x8xy3 - xor vx, vy
	// Set vx = vx ^ vy

	// Performs a bitwise exclusive OR on the values of
	// vx and vy, then stores the result in vx.
	chip.v[instr.x] = (chip.v[instr.x] ^ chip.v[instr.y]);
	increment_pc(chip);
}


void ISA::add_vx_vy(chip8& chip, instruction instr) {

	// 0x8xy4 - add vx, vy
	// Set vx = vx + vy, set vf = carry

	// The values of vx and vy are added together and the result
	// is stored in vx. If the result overflows, vf is set to 1,
	// otherwise 0.is greater than

	//This is not finished! TODO: Find a way to detect overflow.
	chip.v[instr.x] = (chip.v[instr.x] + chip.v[instr.y]);
	chip.v[0xF] = 1;

	increment_pc(chip);
}


void ISA::sub_vx_vy(chip8& chip, instruction instr) {

	// 0x8xy5 - sub vx, vy
	// Set vx = vx - vy, set vf = NOT borrow

	// If vx > vy, then vf is set to 1, otherwise 0. Then vy is
	// subtracted from vx, and the results stored in vx.

	increment_pc(chip);
}


void ISA::shr_vx(chip8& chip, instruction instr) {

	// 0x8xy6 - shr vx {, vy}
	// Set vx = vy >> 1

	// vx is set to the value of vy shifted right by 1. Then, vf
	// is set to the value of the least significant bit of vy.

	chip.v[instr.x] = (chip.v[instr.y] >> 1);
	increment_pc(chip);
}


void ISA::subn_vx_vy(chip8& chip, instruction instr) {

	// 0x8xy7 - subn vx, vy
	// Set vx = vy - vx, set vf = NOT borrow

	// If vy > vx, then vf is set to 1, otherwise 0. Then vx is
	// subtracted from vy, and the results stored in vx.

	increment_pc(chip);
}


void ISA::shl_vx(chip8& chip, instruction instr) {

	// 0x8xyE - shl vx {, vy}
	// Set vx = vy << 1

	// vx is set to the value of vy shifted right by 1. Then, vf
	// is set to the value of the least significant bit of vy.

	chip.v[instr.x] = (chip.v[instr.y] << 1);

	increment_pc(chip);
}


void ISA::sne_vx_vy(chip8& chip, instruction instr) {

	// 0x9xy0 - sne vx, vy
	// Skip next instr if vx != vy

	// The values of vx and vy are compared, and if they are not equal,
	// the program counter is increased by 2.

	if (chip.v[instr.x] != (chip.v[instr.y])) {
		increment_pc(chip);
	}

	increment_pc(chip);
}


void ISA::mov_i_nnn(chip8& chip, instruction instr) {

	// 0xAnnn - mov i, addr
	// Set i = nnn

	// The value of register i is set to nnn.

	chip.i = instr.nnn;
	increment_pc(chip);
}


void ISA::jmp_v0_nnn(chip8& chip, instruction instr) {

	// 0xBnnn - jmp v0, addr
	// Jump to location nnn + V0

	// The program counter is set to nnn plus the value of V0.

	chip.pc = instr.nnn + chip.v[0];
}


void ISA::rnd_vx_nn(chip8& chip, instruction instr) {

	// 0xCxnn - rnd vx, byte
	// Set vx = random byte AND nn

	// The interpreter generates a random number from 0 to 255, which
	// is then ANDed with the value nn. The results are stored in vx.

	int rnd;

	rnd = rand() % 255;

	chip.v[instr.x] = (instr.nn & rnd);

	increment_pc(chip);
}


void ISA::drw_vx_vy_n(chip8& chip, instruction instr) {

	// 0xDxyn - drw vx, vy, n
	// Display n-byte sprite starting at memory location i at (vx, vy), set vf = collision.

	// The interpreter reads n bytes from memory, starting at the address stored in i.
	// These bytes are then displayed as sprites on screen at coordinates (vx, vy).
	// Sprites are XORed onto the existing screen. If this causes any pixels to be erased,
	// vf is set to 1, otherwise it is set to 0. If the sprite is positioned so part of it is
	// outside the coordinates of the display, it wraps around to the opposite side of the screen.

	increment_pc(chip);
}


void ISA::skp_vx(chip8& chip, instruction instr) {

	// 0xEx9E - skp vx
	// Skip next instr if key with the value of vx is pressed

	// Checks the keyboard, and if the key corresponding to the value
	// of vx is currently in the down position, pc is increased by 2.

	increment_pc(chip);
}


void ISA::sknp_vx(chip8& chip, instruction instr) {

	// 0xExA1 - sknp vx
	// Skip next instr if key with the value of vx is not pressed

	// Checks the keyboard, and if the key corresponding to the value
	// of vx is currently in the up position, pc is increased by 2.

	increment_pc(chip);
}


void ISA::gdly_vx(chip8& chip, instruction instr) {

	// 0xFx07 - gdly vx
	// Set vx = delay timer value

	// The value of DT is placed into vx.

	increment_pc(chip);
}


void ISA::key_vx(chip8& chip, instruction instr) {

	// 0xFx0A - key vx
	// Wait for a key press, store the value of the key in vx

	// All execution stops until a key is pressed, then the value
	// of that key is stored in vx.

}


void ISA::sdly_vx(chip8& chip, instruction instr) {

	// 0xFx15 - sdly vx
	// Set delay timer = vx

	// DT is set equal to the value of vx.

	increment_pc(chip);
}


void ISA::ssnd_vx(chip8& chip, instruction instr) {

	// 0xFx18 - ssnd vx
	// Set sound timer = vx

	// ST is set equal to the value of vx.

	increment_pc(chip);
}


void ISA::add_i_vx(chip8& chip, instruction instr) {

	// 0xFx1E - add i, vx
	// Set i = i + vx

	// The values of i and vx are added, and the results are stored in i.
	// vf is set to 1 when there is a range overflow (i+vx > 0xFFF),
	// and to 0 when there isn't.

	increment_pc(chip);
}


void ISA::font_vx(chip8& chip, instruction instr) {

	// Fx29 - font vx
	// Set i = location of sprite for digit vx

	// The value of i is set to the location for the hexadecimal sprite
	// of the character corresponding to the value of vx.

	increment_pc(chip);
}


void ISA::bcd_vx(chip8& chip, instruction instr) {

	// Fx33 - bcd vx
	// Store BCD representation of vx in memory locations i, i+1, and i+2

	// The interpreter takes the decimal value of vx, and places the
	// hundreds digit in memory at location in i, the tens digit at
	// location i+1, and the ones digit at location i+2.

	increment_pc(chip);
}


void ISA::str_v0_vx(chip8& chip, instruction instr) {

	// 0xFx55 - str v0, vx
	// Store registers v0 through vx in memory starting at location i

	// The interpreter copies the values of registers v0 through vx
	// into memory, starting at the address in i. i is set to
	// i + x + 1 after this operation.

	increment_pc(chip);
}


void ISA::ld_v0_vx(chip8& chip, instruction instr) {

	// 0xFx65 - ld_v0_vx
	// Read registers v0 through vx from memory starting at location i

	// The interpreter reads values from memory starting at location i
	// into registers V0 through vx. i is set to i + x + 1 after this
	// operation.

	increment_pc(chip);
}