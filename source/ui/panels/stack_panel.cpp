#include "stack_panel.h"

#include "chip8/chip8.h"
#include "util/strings.h"

#include <algorithm>
#include <format>
#include <string>

#include <imgui_stdlib.h>

namespace c8 {

auto StackPanel::draw(UiContext& context) -> void {
    const float font_size = ImGui::GetFontSize();
    ImGui::SetNextWindowSize({font_size * 9.0f, font_size * 25.0f}, ImGuiCond_FirstUseEver);

    if (ImGui::Begin(title(), &open)) {
        auto& state = context.chip.state();

        ImGui::Text("Stack");
        ImGui::Separator();

        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2{0, 0});

        const int depth = std::min<int>(state.sp, MachineState::stack_depth);
        for (int i = depth - 1; i >= 0; --i) {
            auto& entry = state.stack[static_cast<size_t>(i)];
            auto value_str = std::format("0x{:04X}", entry);

            ImGui::PushID(i);
            ImGui::Text("%02d:", i);
            ImGui::SameLine();

            ImGui::SetNextItemWidth(ImGui::CalcTextSize(value_str.c_str()).x);
            if (ImGui::InputText("##stack", &value_str, ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll)) {
                entry = parse_hex<uint16_t>(value_str).value_or(entry);
            }
            ImGui::PopID();
        }

        ImGui::PopStyleVar();
    }
    ImGui::End();
}

} // namespace c8
