#include "program_panel.h"

#include "assembler/disassembler.h"
#include "chip8/chip8.h"
#include "debugger/debugger.h"

#include <string>

namespace c8 {

namespace {

constexpr ImU32 pc_row_color     = IM_COL32(255, 200, 0, 60);
constexpr ImU32 breakpoint_color = IM_COL32(230, 60, 60, 255);


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
    const uint32_t parity = pc & 1u;
    const int row_count = static_cast<int>(state.memory.size() / 2);
    const int pc_row = static_cast<int>(pc / 2);

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
        if (scroll_to_pc) {
            clipper.IncludeItemByIndex(pc_row);
        }

        while (clipper.Step()) {
            for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row) {
                const auto address = static_cast<uint16_t>((static_cast<uint32_t>(row) * 2) + parity);
                const uint16_t word = state.read_word(address);
                const bool in_program = (address >= program_begin) && (address < program_end);

                ImGui::PushID(row);
                ImGui::TableNextRow();

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
                    const auto text = format_instruction(word, state.read_word(address + 2u), variant);
                    if (in_program) {
                        ImGui::TextUnformatted(text.c_str());
                    }
                    else {
                        ImGui::TextDisabled("%s", text.c_str());
                    }
                }

                // Scroll only if the PC row is out of view, so the listing does not jump at each step
                if (scroll_to_pc && (address == pc)) {
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
