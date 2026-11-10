#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include "assembler/assembler.h"
#include "assembler/source_map.h"
#include "chip8/variant.h"

namespace c8 {

/// How the disassembler finds the instructions
enum class DisassemblyMode : uint8_t {
    flow,   ///< Follow the jumps, calls, and skips from the origin. Unreached bytes become data.
    linear, ///< Decode every word from the origin. No labels.
};

struct DisassemblyOptions {
    uint16_t        origin  = 0x200; ///< The address of the first program byte
    DisassemblyMode mode    = DisassemblyMode::flow;
    Variant         variant = Variant::chip8; ///< The instruction set
};


struct DisassemblyResult {
    /// The source lines. They assemble to the same bytes at the same origin.
    std::vector<std::string> lines;

    /// The map that the assembler makes from the lines
    SourceMap map;

    /// The labels that the assembler defines from the lines, in order of address
    std::vector<Label> labels;

    /// The lines as one text. Each line ends with '\n'.
    [[nodiscard]]
    auto text() const -> std::string;
};


/**
 * @brief The assembly text of one instruction on a platform
 *
 * @details An undefined word is `dw 0x5123`. Addresses are numbers, not
 *          labels. F000 NNNN (XO-CHIP) reads its address from the next word.
 */
[[nodiscard]]
auto format_instruction(uint16_t word, uint16_t next_word, Variant variant) -> std::string;


/**
 * @brief Disassemble a program
 *
 * @details The text starts with a `platform` line if the platform is not
 *          CHIP-8.
 *
 *          In flow mode, the disassembler follows each path of execution
 *          from the origin (see FlowAnalysis):
 *          - `jmp` and `call` targets get the labels `lbl_xxx` and `sub_xxx`.
 *          - A `jmp v0` target gets the label `tbl_xxx`.
 *          - A `mov i` target gets the label `dat_xxx`.
 *
 *          The bytes that no path reaches become `db` lines of up to 8 bytes.
 *          A label never reads as a number, because it has a '_'.
 *
 *          In linear mode, each word is an instruction and there are no
 *          labels. An odd byte at the end becomes a `db` line. On XO-CHIP,
 *          F000 and the word after it are one instruction.
 *
 *          In both modes, the text assembles to the same bytes.
 */
[[nodiscard]]
auto disassemble(std::span<const uint8_t> program, const DisassemblyOptions& options = {}) -> DisassemblyResult;

} // namespace c8
