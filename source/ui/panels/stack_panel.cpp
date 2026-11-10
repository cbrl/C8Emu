#include "stack_panel.h"

#include "app/source_document.h"
#include "chip8/chip8.h"
#include "debugger/address_names.h"
#include "util/strings.h"

#include <algorithm>
#include <format>
#include <string>

#include <imgui_stdlib.h>

namespace c8 {

auto StackPanel::draw(UiContext& context) -> void {
    const float font_size = ImGui::GetFontSize();
    ImGui::SetNextWindowSize({font_size * 14.0f, font_size * 25.0f}, ImGuiCond_FirstUseEver);

    if (!ImGui::Begin(title(), &open)) {
        ImGui::End();
        return;
    }

    auto& state = context.chip.state();
    const auto& labels = context.document.program_labels();

    constexpr auto table_flags = ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_ScrollY;
    if (ImGui::BeginTable("##stack", 3, table_flags)) {
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableSetupColumn("##depth", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn("Address", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn("Location", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableHeadersRow();

        // The current location first, then each return address, newest first
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::TextDisabled("PC");
        ImGui::TableNextColumn();
        ImGui::Text("0x%04X", state.pc);
        ImGui::TableNextColumn();
        ImGui::TextUnformatted(address_name(state.pc, labels).c_str());

        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2{0, 0});

        const int depth = std::min<int>(state.sp, MachineState::stack_depth);
        for (int index = depth - 1; index >= 0; --index) {
            auto& entry = state.stack[static_cast<size_t>(index)];
            auto value_text = std::format("0x{:04X}", entry);

            ImGui::PushID(index);
            ImGui::TableNextRow();

            ImGui::TableNextColumn();
            ImGui::Text("%02d", index);

            ImGui::TableNextColumn();
            ImGui::SetNextItemWidth(ImGui::CalcTextSize(value_text.c_str()).x);
            if (ImGui::InputText("##return", &value_text, ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll)) {
                entry = parse_hex<uint16_t>(value_text).value_or(entry);
                context.requests.state_edited = true;
            }
            ImGui::SetItemTooltip("The return address. Edit it and press Enter.");

            ImGui::TableNextColumn();
            ImGui::TextUnformatted(address_name(entry, labels).c_str());

            ImGui::PopID();
        }

        ImGui::PopStyleVar();
        ImGui::EndTable();
    }

    ImGui::End();
}

} // namespace c8
