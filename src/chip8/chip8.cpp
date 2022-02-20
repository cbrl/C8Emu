#include "chip8.h"
#include "isa/isa.h"

#include <iostream>
#include <fstream>


chip8::chip8() {
	reset();
}


auto chip8::reset() -> void {
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

	// Clear the display
	display.clear();

	// Load the font into memory
	for (size_t i = 0; i < font.size(); ++i) {
		memory[i] = font[i];
	}

	pause();
}


auto chip8::pause() noexcept -> void {
	paused = true;
	//timer.pause();
}


auto chip8::resume() noexcept -> void {
	paused = false;
	//timer.resume();
}


auto chip8::run_cycle() -> void {
	// Update timer
	timer.tick();

	if (pc < rom_end) {  //check that the PC is within the ROM's memory region
		if (breakpoints.contains((pc - rom_start) / 2)) {
			pause();
		}
		else {
			ISA::execute_cycle(*this);
		}
	}
	else {
		std::cout << "PC moved past end of ROM\n";
		pause();
	}
}


auto chip8::load_rom(const std::filesystem::path& file) -> bool {
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

	// Start emulation
	resume();

	return true;
}


auto chip8::load_rom(std::span<const uint16_t> rom_data) -> bool {
	// Ensure the ROM will fit in the memory
	if (rom_data.size_bytes() > (memory.size() - rom_start)) {
        std::cout << "The ROM data is too big to fit in the CHIP8 memory\n"
                  << "  Memory size: " << (memory.size() - rom_start) << '\n'
                  << "     ROM size: " << rom_data.size_bytes() << '\n';

		return false;
	}

	// Reset the device before loading the ROM
	reset();

	// Write the instructions to memory
	for (size_t i = 0; i < rom_data.size(); ++i) {
		const auto idx = rom_start + (i * 2);
		memory[idx]     = static_cast<uint8_t>(rom_data[i] >> 8);
		memory[idx + 1] = static_cast<uint8_t>(rom_data[i]);
	}

	current_rom = std::filesystem::path{};
	rom_end = rom_start + rom_data.size_bytes();

	resume();

	return true;
}
