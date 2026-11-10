#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "chip8/variant.h"

namespace c8 {

/// The role of a program byte after a flow analysis
enum class ByteRole : uint8_t {
    unknown,     ///< No path reached the byte, so it is data
    instruction, ///< The first byte of an instruction
    operand,     ///< Another byte of an instruction
};


/// Why a path reaches an address. When two kinds name the same address, the larger kind wins.
enum class TargetKind : uint8_t {
    data,       ///< A `mov i` address
    jump,       ///< A `jmp` target
    table,      ///< A `jmp v0` base
    subroutine, ///< A `call` target
};


/**
 * @class FlowAnalysis
 *
 * @brief Finds the instructions that the paths of execution from the origin reach
 *
 * @details The analysis decodes the words for one variant and follows each path:
 *          - `jmp` goes to its target. `call` goes to its target and to the
 *            next instruction.
 *          - A skip goes to the next instruction and to the one after it. On
 *            XO-CHIP, a skip over F000 NNNN moves 6 bytes.
 *          - `jmp v0` goes to its base. Each `jmp` in a row from the base is
 *            an entry of a jump table.
 *          - A path stops at `ret`, at `exit`, at the word 0000, at an undefined
 *            word, and at bytes that another instruction uses.
 *          - On Hires CHIP-8, the jump 1260 at 0x200 goes to 0x2C0.
 */
class FlowAnalysis {
public:
    FlowAnalysis(std::span<const uint8_t> program, uint32_t origin, Variant variant);

    /// The role of the byte at an offset from the origin
    [[nodiscard]]
    auto role(size_t offset) const -> ByteRole {
        return roles[offset];
    }

    /// The addresses that the paths name, and why. An address can be outside the program.
    [[nodiscard]]
    auto targets() const noexcept -> const std::map<uint32_t, TargetKind>& {
        return target_kinds;
    }

    /// The program bytes
    [[nodiscard]]
    auto program() const noexcept -> std::span<const uint8_t> {
        return bytes;
    }

    [[nodiscard]]
    auto origin() const noexcept -> uint32_t {
        return start;
    }

    /// The offset of an address, if the program holds `count` bytes from that address
    [[nodiscard]]
    auto offset_of(uint32_t address, size_t count) const -> std::optional<size_t>;

private:
    auto follow(uint32_t address) -> void;
    auto add_target(uint32_t address, TargetKind kind) -> void;
    auto visit(uint32_t address) -> void;
    auto follow_table(uint32_t base) -> void;

    [[nodiscard]]
    auto word_at(size_t offset) const -> uint16_t;

    std::span<const uint8_t> bytes;
    uint32_t start;
    Variant variant;

    std::vector<ByteRole> roles;
    std::vector<uint32_t> pending;
    std::map<uint32_t, TargetKind> target_kinds;
};


/// A platform guess and the reason for it
struct PlatformGuess {
    Variant     variant = Variant::chip8;
    std::string reason; ///< For the user, for example "00FF at 0x0204"
};

/**
 * @brief Guess the platform of a program from its code
 *
 * @details The rules, in order:
 *          1. The jump 1260 at 0x200 gives Hires CHIP-8.
 *          2. A reachable XO-CHIP instruction gives XO-CHIP.
 *          3. A program that ends past 0x1000 gives XO-CHIP.
 *          4. A reachable SUPER-CHIP instruction gives SUPER-CHIP. DXY0 does
 *             not count, because on CHIP-8 it draws nothing.
 *          5. Anything else is CHIP-8.
 */
[[nodiscard]]
auto detect_platform(std::span<const uint8_t> program, uint16_t origin) -> PlatformGuess;

} // namespace c8
