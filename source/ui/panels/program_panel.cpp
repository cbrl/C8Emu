#include "program_panel.h"

#include "app/source_document.h"
#include "chip8/chip8.h"
#include "debugger/debugger.h"

#include <algorithm>
#include <string>

namespace c8 {

namespace {

constexpr ImU32 pc_row_color     = IM_COL32(255, 200, 0, 60);
constexpr ImU32 breakpoint_color = IM_COL32(230, 60, 60, 255);
constexpr auto  label_color      = ImVec4{0.35f, 0.70f, 0.61f, 1.0f};


// The breakpoint dot of a row. Returns true if the user clicked it.
auto breakpoint_dot(bool has_breakpoint) -> bool {
    const float size = ImGui::GetTextLineHeight();
    const ImVec2 pos = ImGui::GetCursorScreenPos();

    const bool clicked = ImGui::InvisibleButton("##breakpoint", {size, size});
    const ImVec2 center = {pos.x + (size * 0.5f), pos.y + (size * 0.5f)};
    const float radius = size * 0.3f;

    auto* draw_list = ImGui::GetWindowDrawList();
    if (has_breakpoint) {
        draw_list->AddCircleFilled(center, radius, breakpoint_color);
    }
    else if (ImGui::IsItemHovered()) {
        draw_list->AddCircle(center, radius, breakpoint_color);
    }

    ImGui::SetItemTooltip("%s", has_breakpoint ? "Remove the breakpoint" : "Add a breakpoint");
    return clicked;
}

} // namespace


auto ProgramPanel::update_rows(const SourceDocument& document, uint32_t parity, size_t memory_size) -> void {
    const auto version = document.program_labels_version();
    if ((version == row_labels) && (parity == row_parity) && (memory_size == row_memory) && !rows.empty()) {
        return;
    }

    labels = document.program_labels();
    row_labels = version;
    row_parity = parity;
    row_memory = memory_size;

    names.clear();
    for (const auto& label : labels) {
        names.try_emplace(static_cast<uint32_t>(label.value), label.name);
    }

    // A label at an address of the other parity is inside a row, so it gets no row
    rows.clear();
    rows.reserve((memory_size / 2) + labels.size());

    size_t next = 0;
    for (uint32_t address = parity; address < memory_size; address += 2) {
        while ((next < labels.size()) && (labels[next].value < address)) {
            ++next;
        }
        while ((next < labels.size()) && (labels[next].value == address)) {
            rows.push_back(Row{.address = static_cast<uint16_t>(address), .label = static_cast<int32_t>(next)});
            ++next;
        }
        rows.push_back(Row{.address = static_cast<uint16_t>(address)});
    }
}


auto ProgramPanel::instruction_row(uint16_t address) const -> int {
    // The label rows of an address come before its instruction row
    auto it = std::ranges::lower_bound(rows, address, {}, &Row::address);
    while ((it != rows.end()) && (it->address == address) && (it->label >= 0)) {
        ++it;
    }
    return static_cast<int>(it - rows.begin());
}


auto ProgramPanel::draw(UiContext& context) -> void {
    if (!ImGui::Begin(title(), &open)) {
        ImGui::End();
        return;
    }

    const auto& state = context.chip.state();
    const auto variant = context.chip.variant();
    auto& debugger = context.debugger;

    ImGui::Checkbox("Follow PC", &follow_pc);
    ImGui::SetItemTooltip("Scroll to the PC when it moves out of view");

    // The rows start at address 0 or 1, the same parity as the PC
    const uint16_t pc = state.pc;
    update_rows(context.document, pc & 1u, state.memory.size());

    const int row_count = static_cast<int>(rows.size());
    const int pc_row = instruction_row(pc);

    const bool pc_moved = (pc != shown_pc);
    shown_pc = pc;
    bool scroll_to_pc = follow_pc && pc_moved;

    const uint32_t program_begin = context.chip.program_origin();
    const uint32_t program_end = context.chip.program_end();

    constexpr auto table_flags = ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV;
    if (ImGui::BeginTable("##listing", 4, table_flags)) {
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableSetupColumn("##breakpoint", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn("Address", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn("Word", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn("Instruction", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableHeadersRow();

        auto clipper = ImGuiListClipper{};
        clipper.Begin(row_count);
        if (scroll_to_pc && (pc_row < row_count)) {
            clipper.IncludeItemByIndex(pc_row);
        }

        while (clipper.Step()) {
            for (int index = clipper.DisplayStart; index < clipper.DisplayEnd; ++index) {
                const auto& row = rows[static_cast<size_t>(index)];
                const uint16_t address = row.address;

                ImGui::PushID(index);
                ImGui::TableNextRow();

                if (row.label >= 0) {
                    ImGui::TableSetColumnIndex(3);
                    const auto text = labels[static_cast<size_t>(row.label)].name + ':';
                    ImGui::TextColored(label_color, "%s", text.c_str());
                    ImGui::PopID();
                    continue;
                }

                const uint16_t word = state.read_word(address);
                const bool in_program = (address >= program_begin) && (address < program_end);

                if (address == pc) {
                    ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg1, pc_row_color);
                }

                ImGui::TableNextColumn();
                if (breakpoint_dot(debugger.has_breakpoint(address))) {
                    (void)debugger.toggle_breakpoint(address);
                }

                ImGui::TableNextColumn();
                ImGui::Text("0x%04X", address);

                ImGui::TableNextColumn();
                ImGui::Text("%04X", word);

                ImGui::TableNextColumn();

                // On XO-CHIP, the word after F000 is the address of the instruction, not an instruction
                const bool long_operand = has_xochip(variant) && (state.read_word(address - 2u) == 0xF000);
                if (long_operand) {
                    ImGui::TextDisabled("(the address of F000)");
                }
                else {
                    const auto text = format_instruction(word, state.read_word(address + 2u), variant, &names);
                    if (in_program) {
                        ImGui::TextUnformatted(text.c_str());
                    }
                    else {
                        ImGui::TextDisabled("%s", text.c_str());
                    }
                }

                // Scroll only if the PC row is out of view, so the listing does not jump at each step
                if (scroll_to_pc && (index == pc_row)) {
                    if (!ImGui::IsItemVisible()) {
                        ImGui::SetScrollHereY(0.25f);
                    }
                    scroll_to_pc = false;
                }

                ImGui::PopID();
            }
        }

        ImGui::EndTable();
    }

    ImGui::End();
}

} // namespace c8
