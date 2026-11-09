#include "breakpoints_panel.h"

#include "assembler/disassembler.h"
#include "chip8/chip8.h"
#include "debugger/debugger.h"
#include "util/strings.h"

#include <cstdint>
#include <format>
#include <optional>
#include <vector>

#include <imgui_stdlib.h>

namespace c8 {

auto BreakpointsPanel::draw(UiContext& context) -> void {
    if (!ImGui::Begin(title(), &open)) {
        ImGui::End();
        return;
    }

    auto& debugger = context.debugger;
    const auto& state = context.chip.state();

    // Add a breakpoint with Enter or with the button
    ImGui::SetNextItemWidth(ImGui::GetFontSize() * 8.0f);
    bool add = ImGui::InputTextWithHint("##address", "Address (hex)", &address_text, ImGuiInputTextFlags_EnterReturnsTrue);
    if (add) {
        // Keep the focus in the field for the next address
        ImGui::SetKeyboardFocusHere(-1);
    }

    ImGui::SameLine();
    add |= ImGui::Button("Add");
    if (add) {
        add_from_text(context);
    }

    ImGui::SameLine();
    ImGui::BeginDisabled(debugger.breakpoints().empty());
    if (ImGui::Button("Remove All")) {
        debugger.clear_breakpoints();
    }
    ImGui::EndDisabled();

    if (!error.empty()) {
        ImGui::TextColored(ImVec4{1.0f, 0.4f, 0.4f, 1.0f}, "%s", error.c_str());
    }

    ImGui::Separator();

    if (debugger.breakpoints().empty()) {
        ImGui::TextDisabled("No breakpoints. Click the dots in the Program panel, or enter an address.");
        ImGui::End();
        return;
    }

    constexpr auto table_flags = ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_ScrollY;
    if (ImGui::BeginTable("##breakpoints", 3, table_flags)) {
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableSetupColumn("Address", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn("Instruction", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("##remove", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableHeadersRow();

        // Remove after the loop, so the loop does not change the set that it reads
        auto removed = std::optional<uint16_t>{};

        for (const auto address : debugger.breakpoints()) {
            ImGui::PushID(address);
            ImGui::TableNextRow();

            // The machine stopped at this breakpoint
            if (!debugger.is_running() && (state.pc == address)) {
                ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg1, IM_COL32(255, 200, 0, 50));
            }

            ImGui::TableNextColumn();
            ImGui::Text("0x%04X", address);

            ImGui::TableNextColumn();
            const auto text = format_instruction(state.read_word(address));
            ImGui::TextUnformatted(text.c_str());

            ImGui::TableNextColumn();
            if (ImGui::SmallButton("Remove")) {
                removed = address;
            }

            ImGui::PopID();
        }

        ImGui::EndTable();

        if (removed) {
            debugger.remove_breakpoint(*removed);
        }
    }

    ImGui::End();
}


auto BreakpointsPanel::add_from_text(UiContext& context) -> void {
    const auto memory_size = context.chip.state().memory.size();
    const auto address = parse_hex<uint16_t>(address_text);

    if (!address || (*address >= memory_size)) {
        error = std::format("Enter a hex address from 0x000 to 0x{:03X}", memory_size - 1);
        return;
    }

    context.debugger.add_breakpoint(*address);
    address_text.clear();
    error.clear();
}

} // namespace c8
