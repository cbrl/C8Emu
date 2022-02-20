#pragma once

#include <array>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <span>
#include <unordered_set>
#include <vector>

#include "display/display.h"
#include "input/input.h"
#include "timer/chip_timer.h"


class chip8 {
    friend class ISA;
    friend class MediaLayer;

public:

    chip8();

    /// Resets the state of the system
	auto reset() -> void;

    /// Pause execution
	auto pause() noexcept -> void;

    [[nodiscard]]
    auto is_paused() const noexcept -> bool {
        return paused;
    }

    /// Resume execution
	auto resume() noexcept -> void;

    /// Run a single cycle of the syystem
    auto run_cycle() -> void;

    /**
     * @brief Load a ROM into memory
     * 
     * @details Resets the state of the system and loads the ROM.
     *          If the file does not exist, cannot be opened, or is
     *          too large for the memory, then the function will exit
     *          early without resetting the system.
     * 
     * @param[in] file  The path to the ROM file
     * 
     * @return True if the file loaded without error, otherwise false.
     */
    [[nodiscard]]
    auto load_rom(const std::filesystem::path& file) -> bool;

    /**
     * @brief Load a ROM into memory
     *
     * @details Resets the state of the system and loads the ROM.
     *          If the ROM is too large for the memory, then the
     *          function will exit early without resetting the system.
     *
     * @param[in] instructions  The list of instructions to write into memory
     * 
     * @return True if the file loaded without error, otherwise false.
     */
    [[nodiscard]]
    auto load_rom(std::span<const uint16_t> rom_data) -> bool;

    [[nodiscard]]
    auto get_clock_rate() const noexcept -> uint32_t {
        return clock_rate;
    }
    auto set_clock_rate(uint32_t rate) noexcept -> void {
        clock_rate = rate;
    }

    /// Get the legacy mode status. Legacy mode changes the behavior of certain instructions. Newer ROMS might not expect legacy behavior.
    [[nodiscard]]
    auto is_legacy_mode() const noexcept -> bool {
        return legacy_mode;
    }

    /// Enable/disable legacy mode, which changes the behavior of certain instructions. Newer ROMS might not expect legacy behavior.
    auto set_legacy_mode(bool state) noexcept -> void {
        legacy_mode = state;
    }

    /**
    * @brief Add a breakpoint at the specified instruction number
    * @param instruction_number  The instruction number to break at. 0 specifies the first instruction in the ROM.
    */
    auto add_breakpoint(uint16_t instruction_number) -> void {
        breakpoints.insert(instruction_number);
    }
    auto remove_breakpoint(uint16_t instruction_number) -> void {
        breakpoints.erase(instruction_number);
    }
    auto clear_breakpoints() -> void {
        breakpoints.clear();
    }

    [[nodiscard]]
    auto get_breakpoints() const noexcept -> const std::unordered_set<uint16_t>& {
        return breakpoints;
    }

private:

    //--------------------------------------------------------------------------------
    // Execution State
    //--------------------------------------------------------------------------------

    // Enables legacy mode. Modifies the behavior of some instructions that
    // function differently according to the S-CHIP documentation.
    bool legacy_mode = true;

	// The currently loaded ROM and its size
	std::filesystem::path current_rom;

	// Pauses execution when true
	bool paused = false;

    // The clock speed in Hz
    uint32_t clock_rate = 500;

    // Instruction numbers to pause execution at
    std::unordered_set<uint16_t> breakpoints;


    //--------------------------------------------------------------------------------
    // Processor State
    //--------------------------------------------------------------------------------

    // System memory
    std::array<uint8_t, 4096> memory;
	static const size_t rom_start = 512;
    size_t rom_end = rom_start;

    // Registers
    uint16_t pc = rom_start;
    uint16_t i = 0;
    std::array<uint8_t, 16> v;

    // Stack
    std::vector<uint16_t> stack;

    // Input handler
    Input input;

    // Display
    Display<64, 32> display;

    // Delay and Sound timers
    Chip8Timer timer;

    // Font
    static inline const std::array<uint8_t, 80> font = {
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