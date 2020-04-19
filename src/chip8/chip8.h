#pragma once

#include <cstdint>
#include <array>
#include <stack>
#include <filesystem>
#include <functional>

#include "input/input.h"
#include "display/display.h"
#include "timer/chip_timer.h"


class chip8 {
    friend class ISA;

public:

	void reset();

	void pause();

	void resume();

    void run_cycle();

    void load_rom(const std::filesystem::path& file);

private:

	// The currently loaded ROM and its size
	std::filesystem::path current_rom;

	// Pauses execution when true
	bool paused = false;

    // System memory
    std::array<uint8_t, 4096> memory;
	static const size_t rom_start = 512;
    size_t rom_end = rom_start;

    // Registers
    uint16_t pc = rom_start;
    uint16_t i = 0;
    std::array<uint8_t, 16> v;

    // Stack
    std::stack<uint16_t> stack;

    // Input handler
    Input input;

    // Display
    Display<64, 32> display;

    // Delay and Sound timers
    Chip8Timer timer;

    // Font
    const std::array<uint8_t, 80> font = {
        0xF0, 0x90, 0x90, 0x90, 0xF0, // 0
        0x20, 0x60, 0x20, 0x20, 0x70, // 1
        0xF0, 0x10, 0xF0, 0x80, 0xF0, // 2
        0xF0, 0x10, 0xF0, 0x10, 0xF0, // 3
        0x90, 0x90, 0xF0, 0x10, 0x10, // 4
        0xF0, 0x80, 0xF0, 0x10, 0xF0, // 5
        0xF0, 0x80, 0xF0, 0x90, 0xF0, // 6
        0xF0, 0x10, 0x20, 0x40, 0x40, // 7
        0xF0, 0x90, 0xF0, 0x90, 0xF0, // 8
        0xF0, 0x90, 0xF0, 0x10, 0xF0, // 9
        0xF0, 0x90, 0xF0, 0x90, 0x90, // A
        0xE0, 0x90, 0xE0, 0x90, 0xE0, // B
        0xF0, 0x80, 0x80, 0x80, 0xF0, // C
        0xE0, 0x90, 0x90, 0x90, 0xE0, // D
        0xF0, 0x80, 0xF0, 0x80, 0xF0, // E
        0xF0, 0x80, 0xF0, 0x80, 0x80  // F
    };
};