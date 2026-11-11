#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "assembler/assembler.h"
#include "assembler/disassembler.h"
#include "ui/panel.h"

namespace c8 {

/**
 * @class ProgramPanel
 *
 * @brief A listing of all memory as instructions
 *
 * @details Each row is one instruction. The rows start at an even or an odd
 *          address, the same as the PC, so the PC row always decodes
 *          correctly. A click on the dot of a row toggles a breakpoint. A
 *          click on a row shows its line in the Code Editor, and a right
 *          click opens the address menu.
 *
 *          The labels of the program that loaded from the code editor show
 *          as rows before their addresses, and as names in the instructions.
 */
class ProgramPanel final : public Panel {
public:
    [[nodiscard]]
    auto title() const -> const char* override {
        return "Program";
    }

    auto draw(UiContext& context) -> void override;

    /// Bring the panel to the front, select the row of an address, and scroll to it
    auto show(uint32_t address) -> void {
        bring_to_front();
        selected = address;
        pending_scroll = address;
    }

private:
    // One row of the listing: an instruction, or a label before the instruction at its address
    struct Row {
        uint16_t address = 0;
        int32_t  label   = -1; // The index in `labels` for a label row. -1 for an instruction row.
    };

    // Make the rows again if the labels, the parity of the rows, or the memory size changed
    auto update_rows(const SourceDocument& document, uint32_t parity, size_t memory_size) -> void;

    // The row of the instruction at an address with the parity of the rows
    [[nodiscard]]
    auto instruction_row(uint16_t address) const -> int;

    // The field that selects a row from a debugger expression
    auto draw_go_to(UiContext& context) -> void;

    // Scroll to the PC row when the PC changes and the row is not visible
    bool follow_pc = true;

    // The PC in the last frame. A different PC scrolls the listing if follow_pc is on.
    uint32_t shown_pc = 0x10000;

    // The address of the selected row, and an address to scroll to in the next frame
    std::optional<uint32_t> selected;
    std::optional<uint32_t> pending_scroll;

    std::string go_to_text;
    std::string go_to_error;

    std::vector<Label> labels;      // The labels of the rows, in order of address
    AddressNames       names;       // The first label of each address
    std::vector<Row>   rows;
    uint64_t           row_labels  = 0; // The version of the program labels of the rows
    uint32_t           row_parity  = 2; // 2: no rows yet
    size_t             row_memory  = 0;
};

} // namespace c8
