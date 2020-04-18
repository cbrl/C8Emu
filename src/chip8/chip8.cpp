#include "chip8.h"
#include "isa/isa.h"

#include <iostream>
#include <fstream>


void chip8::reset() {
	pc = 0;
	i = 0;
	v.fill(0);
	memory.fill(0);
	current_rom = std::filesystem::path{};

	while (!stack.empty()) {
		stack.pop();
	}
}


void chip8::run_cycle() {
	if (paused) {
		return;
	}

	ISA::execute_cycle(*this);
}


void chip8::load_rom(const std::filesystem::path& file) {
    // Check that file exists
    if (!std::filesystem::exists(file)) {
        std::cout << "Error loading ROM " << file << ": The file does not exist\n";
        return;
    }

    // Ensure the ROM will fit in the memory
	const auto file_size = std::filesystem::file_size(file);
    if (file_size > memory.size()) {
        std::cout << "The ROM " << file << " is too big to fit in the CHIP8 memory\n"
                  << "  Memory size: " << memory.size() << '\n'
                  << "     ROM size: " << file_size << '\n';
        return;
    }

	// Open the file and check that it's not in an error state
    std::ifstream rom(file, std::ios::binary);
	if (!rom) {
        std::cout << "Error opening " << file << '\n';
        return;
    }

	// Reset the device before loading the ROM
	reset();

	// Read the entire file contents into the memory array
    rom.read(reinterpret_cast<char*>(memory.data()), file_size);
	current_rom = file;
}
