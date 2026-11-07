#pragma once

#include <array>
#include <cstdint>
#include <string_view>
#include <vector>

#include "display/display.h"

namespace c8 {

/// An error that stops execution. The instruction that causes it does not change the machine state.
enum class Fault : uint8_t {
    none,
    stack_overflow,      ///< 2NNN with a full stack
    stack_underflow,     ///< 00EE with an empty stack
    pc_out_of_range,     ///< The PC points past the end of memory
    illegal_instruction, ///< The word at the PC is not a CHIP-8 instruction
};

[[nodiscard]]
constexpr auto to_string(Fault fault) noexcept -> std::string_view {
    switch (fault) {
        case Fault::none:                return "none";
        case Fault::stack_overflow:      return "stack overflow";
        case Fault::stack_underflow:     return "stack underflow";
        case Fault::pc_out_of_range:     return "PC out of range";
        case Fault::illegal_instruction: return "illegal instruction";
    }
    return "unknown";
}


/// A fault and where it happened
struct FaultInfo {
    Fault    kind   = Fault::none;
    uint16_t pc     = 0;
    uint16_t opcode = 0;

    friend constexpr auto operator==(const FaultInfo&, const FaultInfo&) -> bool = default;
};


/// The phase of an FX0A key wait
enum class KeyWaitPhase : uint8_t {
    idle,         ///< No FX0A is in progress
    wait_press,   ///< FX0A waits for a key press
    wait_release, ///< FX0A waits for the release of the pressed key (key_wait_release quirk)
};


/// The progress of an FX0A instruction
struct KeyWait {
    KeyWaitPhase phase   = KeyWaitPhase::idle;
    uint8_t      reg     = 0; ///< X of the FX0A instruction
    uint8_t      key     = 0; ///< The pressed key
    uint16_t     pc      = 0; ///< The address of the FX0A instruction
    uint16_t     ignored = 0; ///< Keys held when the wait started. A key is ignored until it is released.

    friend constexpr auto operator==(const KeyWait&, const KeyWait&) -> bool = default;
};


/**
 * @struct MachineState
 *
 * @brief The complete state of a CHIP-8 machine
 */
struct MachineState {
    static constexpr uint16_t program_start = 0x200;
    static constexpr uint16_t font_address  = 0x000;
    static constexpr uint8_t  stack_depth   = 16;

    friend auto operator==(const MachineState&, const MachineState&) -> bool = default;

    /// The mask that keeps an address inside the valid memory range.
    [[nodiscard]]
    auto mask() const noexcept -> uint32_t {
        return static_cast<uint32_t>(memory.size() - 1);
    }

    /// Read a byte. The address wraps at the end of memory.
    [[nodiscard]]
    auto read(uint32_t address) const noexcept -> uint8_t {
        return memory[address & mask()];
    }

    /// Write a byte. The address wraps at the end of memory.
    auto write(uint32_t address, uint8_t value) noexcept -> void {
        memory[address & mask()] = value;
    }

    /// Read the big-endian word at an address
    [[nodiscard]]
    auto read_word(uint32_t address) const noexcept -> uint16_t {
        return static_cast<uint16_t>((read(address) << 8) | read(address + 1));
    }

    /// True while key (0x0-0xF) is held
    [[nodiscard]]
    auto key_down(uint8_t key) const noexcept -> bool {
        return ((keys >> (key & 0xF)) & 1) != 0;
    }

    /// True while the FX0A instruction at the PC waits for a key
    [[nodiscard]]
    auto in_key_wait() const noexcept -> bool {
        return (key_wait.phase != KeyWaitPhase::idle)
            && (key_wait.pc == pc)
            && (read_word(pc) == static_cast<uint16_t>(0xF00A | (key_wait.reg << 8)));
    }

    // Memory. The size is a power of two, so an address mask keeps every access in range.
    std::vector<uint8_t> memory = std::vector<uint8_t>(4096);

    // Registers
    std::array<uint8_t, 16> v{};
    uint16_t i  = 0;
    uint16_t pc = program_start;

    // Return addresses. sp is the number of entries in use.
    std::array<uint16_t, stack_depth> stack{};
    uint8_t sp = 0;

    // 60 Hz timers
    uint8_t delay_timer = 0;
    uint8_t sound_timer = 0;

    // Key state. Bit n is set while key n is held.
    uint16_t keys = 0;
    KeyWait key_wait;

    // Frame state. A frame is instructions_per_frame cycles. The timers decrement at the end of each frame.
    bool     vblank_wait    = false; ///< A draw waits for the end of the frame (display_wait quirk)
    uint32_t cycle_in_frame = 0;     ///< Cycles done in the current frame
    uint64_t frame_count    = 0;     ///< Frames completed since the reset
    uint64_t total_cycles   = 0;     ///< Instructions executed since the reset

    // xorshift32 state for CXNN. It must not be 0.
    uint32_t rng = 1;

    Display display;
};

} // namespace c8
