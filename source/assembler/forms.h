#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>

namespace c8 {

/// One operand of an instruction form
enum class OperandKind : uint8_t {
    vx,  ///< A V register in bits 8-11
    vy,  ///< A V register in bits 4-7
    v0,  ///< The register v0 only. It sets no bits.
    i,   ///< The register I. It sets no bits.
    nnn, ///< A 12-bit address or a label
    nn,  ///< An 8-bit value
    n,   ///< A 4-bit value
};


/**
 * @struct InstructionForm
 *
 * @brief Defines how to read and write an instruction
 *
 * @details The table of instruction forms drives the assembler, the
 *          disassembler, the syntax highlighting, and the syntax reference
 *          page. A mnemonic can have more than one form, for example
 *          `se vX, nn` and `se vX, vY`.
 */
struct InstructionForm {
    std::string_view mnemonic;
    std::array<OperandKind, 3> operand_list;
    uint8_t operand_count = 0;

    uint16_t opcode = 0; ///< The fixed bits of the instruction word
    uint16_t mask   = 0; ///< The bits that the opcode fixes

    std::string_view pattern; ///< The instruction word, for example "8XY4"
    std::string_view summary; ///< What the instruction does

    /// The one-operand shift: the assembler copies X into Y
    bool y_copies_x = false;

    [[nodiscard]]
    auto operands() const noexcept -> std::span<const OperandKind> {
        return std::span{operand_list}.first(operand_count);
    }
};


/// All instruction forms, in opcode order
[[nodiscard]]
auto instruction_forms() noexcept -> std::span<const InstructionForm>;

/**
 * @brief The form of an instruction word
 *
 * @details The form with the most fixed bits wins, so 00E0 is `cls`, not
 *          `sys`. The one-operand shift forms are only a short way to write
 *          `shr vX, vX` and `shl vX, vX`, so they never match.
 *
 * @return The form, or nullptr if the word is not an instruction
 */
[[nodiscard]]
auto decode_form(uint16_t word) noexcept -> const InstructionForm*;

/// True if a lowercase word is an instruction name
[[nodiscard]]
auto is_mnemonic(std::string_view lowercase_word) noexcept -> bool;

/// True if a lowercase word is a directive (db, dw, org)
[[nodiscard]]
auto is_directive(std::string_view lowercase_word) noexcept -> bool;

/// The syntax of a form, for example "drw vX, vY, n"
[[nodiscard]]
auto format_syntax(const InstructionForm& form) -> std::string;

/// The name of an operand kind in the syntax, for example "vX"
[[nodiscard]]
auto to_string(OperandKind kind) noexcept -> std::string_view;

} // namespace c8
