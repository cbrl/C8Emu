#include "registers_panel.h"

#include "chip8/chip8.h"
#include "util/strings.h"

#include <format>
#include <string>

#include <imgui_stdlib.h>

namespace c8 {

namespace {

constexpr auto edit_flags = ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll;

// An input field as wide as its text. Returns the new text after Enter.
auto hex_field(const char* id, std::string& text) -> bool {
    ImGui::SetNextItemWidth(ImGui::CalcTextSize(text.c_str()).x);
    return ImGui::InputText(id, &text, edit_flags);
}

} // namespace


auto RegistersPanel::draw(UiContext& context) -> void {
    if (ImGui::Begin(title(), &open, ImGuiWindowFlags_AlwaysAutoResize)) {
        auto& state = context.chip.state();

        ImGui::Text("Registers");
        ImGui::Separator();

        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2{0, 0});

        // V registers
        for (uint8_t i = 0; i <= 0xF; ++i) {
            auto value_str = std::format("0x{:02X}", state.v[i]);
            ImGui::PushID(i);

            ImGui::Text("v%X:", i);
            ImGui::SameLine();
            if (hex_field("##reg_v", value_str)) {
                state.v[i] = str_to<uint8_t>(value_str, 16).value_or(state.v[i]);
            }

            ImGui::PopID();
        }
        ImGui::Separator();

        // I register
        auto reg_i_str = std::format("0x{:04X}", state.i);
        ImGui::Text(" I:");
        ImGui::SameLine();
        if (hex_field("##reg_i", reg_i_str)) {
            state.i = str_to<uint16_t>(reg_i_str, 16).value_or(state.i);
        }

        // Program counter
        auto pc_str = std::format("0x{:04X}", state.pc);
        ImGui::Text("PC:");
        ImGui::SameLine();
        if (hex_field("##pc", pc_str)) {
            state.pc = str_to<uint16_t>(pc_str, 16).value_or(state.pc);
        }

        ImGui::PopStyleVar();
    }
    ImGui::End();
}

} // namespace c8
