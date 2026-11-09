#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include "assembler/assembler.h"
#include "assembler/source_map.h"

namespace c8 {

/// How the disassembler finds the instructions
enum class DisassemblyMode : uint8_t {
    flow,   ///< Follow the jumps, calls, and skips from the origin. Unreached bytes become data.
    linear, ///< Decode every word from the origin. No labels.
};

struct DisassemblyOptions {
    uint16_t        origin = 0x200; ///< The address of the first program byte
    DisassemblyMode mode   = DisassemblyMode::flow;
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
 * @brief The assembly text of one instruction word
 *
 * @details An undefined word is `dw 0x5123`. Addresses are numbers, not labels.
 */
[[nodiscard]]
auto format_instruction(uint16_t word) -> std::string;


/**
 * @brief Disassemble a program
 *
 * @details In flow mode, the disassembler follows each path of execution
 *          from the origin:
 *          - `jmp` and `call` targets get the labels `lbl_xxx` and `sub_xxx`.
 *            A call also continues at the next instruction.
 *          - A skip continues at the next instruction and the one after it.
 *          - A `jmp v0` target gets the label `tbl_xxx`. Each `jmp` in a row
 *            from that address is an entry of a jump table.
 *          - A path stops at `ret`, at the word 0000, and at an undefined word.
 *          - A `mov i` target gets the label `dat_xxx`.
 *
 *          The bytes that no path reaches become `db` lines of up to 8 bytes.
 *          A label never reads as a number, because it has a '_'.
 *
 *          In linear mode, each word is an instruction and there are no
 *          labels. An odd byte at the end becomes a `db` line.
 *
 *          In both modes, the text assembles to the same bytes.
 */
[[nodiscard]]
auto disassemble(std::span<const uint8_t> program, const DisassemblyOptions& options = {}) -> DisassemblyResult;

} // namespace c8
