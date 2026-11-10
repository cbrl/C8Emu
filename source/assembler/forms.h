#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>

#include "assembler/lexer.h"
#include "chip8/isa/decode.h"
#include "chip8/variant.h"

namespace c8 {

/// One operand of an instruction form
enum class OperandKind : uint8_t {
    vx,      ///< A V register in bits 8-11
    vy,      ///< A V register in bits 4-7
    v0,      ///< The register v0 only. It sets no bits.
    i,       ///< The register I. It sets no bits.
    long_kw, ///< The keyword `long`. It sets no bits.
    nnn,     ///< A 12-bit address or a label
    nn,      ///< An 8-bit value
    n,       ///< A 4-bit value in bits 0-3
    nx,      ///< A 4-bit value in bits 8-11 (the planes of FN01)
    nnnn,    ///< A 16-bit address or a label in a second word
};


/// The variants that have an instruction, as bits
enum VariantMask : uint8_t {
    on_chip8  = 1u << static_cast<int>(Variant::chip8),
    on_hires  = 1u << static_cast<int>(Variant::chip8_hires),
    on_schip  = 1u << static_cast<int>(Variant::schip),
    on_xochip = 1u << static_cast<int>(Variant::xochip),
    on_all    = on_chip8 | on_hires | on_schip | on_xochip,
    on_super  = on_schip | on_xochip,
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

    uint16_t opcode = 0; ///< The fixed bits of the first instruction word
    uint16_t mask   = 0; ///< The bits that the opcode fixes

    isa::Op op       = isa::Op::invalid; ///< The operation that the word decodes to
    uint8_t variants = on_all;           ///< The VariantMask bits of the platforms that have the instruction

    std::string_view pattern; ///< The instruction word, for example "8XY4"
    std::string_view summary; ///< What the instruction does

    /// The one-operand shift: the assembler copies X into Y
    bool y_copies_x = false;

    [[nodiscard]]
    auto operands() const noexcept -> std::span<const OperandKind> {
        return std::span{operand_list}.first(operand_count);
    }

    [[nodiscard]]
    auto available_on(Variant variant) const noexcept -> bool {
        return (variants & (1u << static_cast<int>(variant))) != 0;
    }

    /// The size of the instruction in bytes: 4 for F000 NNNN, otherwise 2
    [[nodiscard]]
    auto size() const noexcept -> uint16_t {
        return isa::instruction_size(op);
    }
};


/// A directive of the assembler
struct DirectiveInfo {
    std::string_view name;    ///< Lowercase, for example "ds"
    std::string_view syntax;  ///< For example "ds count [fill]"
    std::string_view summary; ///< What the directive does
};


/// All instruction forms, in opcode order
[[nodiscard]]
auto instruction_forms() noexcept -> std::span<const InstructionForm>;

/// All directives
[[nodiscard]]
auto directive_infos() noexcept -> std::span<const DirectiveInfo>;

/// The directive with a lowercase name. nullptr if the word is not a directive.
[[nodiscard]]
auto find_directive(std::string_view lowercase_word) noexcept -> const DirectiveInfo*;

/**
 * @brief The form of an instruction word on a platform
 *
 * @details The form has the operation that the word decodes to, and its fixed
 *          bits match the word. So the text of the form assembles back to the
 *          same word. The one-operand shift forms are only a short way to write
 *          `shr vX, vX` and `shl vX, vX`, so they never match.
 *
 * @return The form, or nullptr if the word is not an instruction on the platform
 */
[[nodiscard]]
auto form_for_word(uint16_t word, Variant variant) noexcept -> const InstructionForm*;

/// True if a lowercase word is an instruction name on any platform
[[nodiscard]]
auto is_mnemonic(std::string_view lowercase_word) noexcept -> bool;

/// True if a lowercase word is a directive (db, dw, ds, org, align, equ, platform)
[[nodiscard]]
auto is_directive(std::string_view lowercase_word) noexcept -> bool;

/**
 * @brief True if a word of a kind can fill an operand of a form
 *
 * @details A register or `long` fills only its own operand kind. Any other
 *          word is a value: a number, a name, or an expression.
 */
[[nodiscard]]
auto operand_fits(OperandKind kind, WordKind word) noexcept -> bool;

/// The syntax of a form, for example "drw vX, vY, n"
[[nodiscard]]
auto format_syntax(const InstructionForm& form) -> std::string;

/// The name of an operand kind in the syntax, for example "vX"
[[nodiscard]]
auto to_string(OperandKind kind) noexcept -> std::string_view;

/// The platforms in a VariantMask, for example "SUPER-CHIP or XO-CHIP"
[[nodiscard]]
auto platform_list(uint8_t variants) -> std::string;

} // namespace c8
