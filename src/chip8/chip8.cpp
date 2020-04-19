#include "chip8.h"
#include "isa/isa.h"

#include <iostream>
#include <fstream>


void chip8::reset() {
	// Reset registers
	pc = rom_start;
	i = 0;
	v.fill(0);

	// Zero out memory
	memory.fill(0);

	// Reset ROM patch
	current_rom = std::filesystem::path{};
	rom_end = rom_start;

	// Empty the stack
	stack.clear();

	// Load the font into memory
	for (size_t i = 0; i < font.size(); ++i) {
		memory[i] = font[i];
	}
}


void chip8::pause() {
	paused = true;
	timer.pause();
}


void chip8::resume() {
	paused = false;
	timer.resume();
}


void chip8::run_cycle() {
	// Update timer
	timer.tick();

	if (paused) return;

	if (pc < rom_end) {  //check that the PC is within the ROM's memory region
		ISA::execute_cycle(*this);
	}
	else {
		std::cout << "PC moved past end of ROM\n";
		pause();
	}
}


bool chip8::load_rom(const std::filesystem::path& file) {
    // Check that file exists
    if (!std::filesystem::exists(file)) {
        std::cout << "Error loading ROM " << file << ": The file does not exist\n";
        return false;
    }

    // Ensure the ROM will fit in the memory
	const auto file_size = std::filesystem::file_size(file);

    if (file_size > (memory.size() - rom_start)) {
        std::cout << "The ROM " << file << " is too big to fit in the CHIP8 memory\n"
                  << "  Memory size: " << (memory.size() - rom_start) << '\n'
                  << "     ROM size: " << file_size << '\n';
        return false;
    }

	// Open the file and check that it's not in an error state
    std::ifstream rom(file, std::ios::binary);
	if (!rom) {
        std::cout << "Error opening " << file << '\n';
        return false;
    }

	// Reset the device before loading the ROM
	reset();

	// Read the entire file contents into the memory array
    rom.read(reinterpret_cast<char*>(memory.data() + rom_start), file_size);
	current_rom = file;
	rom_end = rom_start + file_size;

	return true;
}
