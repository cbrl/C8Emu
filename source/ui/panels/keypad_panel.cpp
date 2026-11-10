#include "keypad_panel.h"

#include "chip8/chip8.h"
#include "ui/dock_layout.h"

#include <algorithm>
#include <string_view>

namespace c8 {

namespace {

constexpr auto digits = std::string_view{"0123456789ABCDEF"};

} // namespace


auto KeypadPanel::draw(UiContext& context) -> void {
    dock_with_on_first_use("Chip8 Settings");

    if (!ImGui::Begin(title(), &open)) {
        ImGui::End();
        return;
    }

    const auto& chip = context.chip;
    const auto& state = chip.state();
    const bool waiting = chip.waiting_for_key();
    const bool wait_release = waiting && (state.key_wait.phase == KeyWaitPhase::wait_release);

    // The keys fill the panel, but stay square
    const auto& style = ImGui::GetStyle();
    const float gap = style.ItemSpacing.x;
    const ImVec2 available = ImGui::GetContentRegionAvail();
    const float text_height = ImGui::GetTextLineHeightWithSpacing() * 1.5f;
    const float fit = std::min(available.x - (gap * 3.0f), available.y - text_height - (gap * 3.0f)) / 4.0f;
    const float cell = std::clamp(fit, ImGui::GetFrameHeight(), ImGui::GetFontSize() * 5.0f);

    uint16_t mouse_keys = 0;

    for (size_t index = 0; index < keypad_layout.size(); ++index) {
        const uint8_t key = keypad_layout[index];
        if ((index % 4) != 0) {
            ImGui::SameLine(0.0f, gap);
        }

        const bool held = state.key_down(key);
        const bool awaited = wait_release && (state.key_wait.key == key);

        ImGui::PushID(key);
        int colors = 0;
        if (held) {
            ImGui::PushStyleColor(ImGuiCol_Button, style.Colors[ImGuiCol_ButtonActive]);
            ++colors;
        }
        else if (awaited) {
            ImGui::PushStyleColor(ImGuiCol_Button, style.Colors[ImGuiCol_PlotHistogram]);
            ++colors;
        }

        const char label[2] = {digits[key], '\0'};
        ImGui::Button(label, {cell, cell});
        if (ImGui::IsItemActive()) {
            mouse_keys = static_cast<uint16_t>(mouse_keys | (1u << key));
        }
        ImGui::SetItemTooltip("Key %X (keyboard: %s)", static_cast<unsigned>(key), context.key_names[key].c_str());

        ImGui::PopStyleColor(colors);
        ImGui::PopID();
    }

    context.requests.keypad_keys = mouse_keys;

    if (wait_release) {
        ImGui::Text("FX0A waits for the release of key %X (into v%X)",
            static_cast<unsigned>(state.key_wait.key), static_cast<unsigned>(state.key_wait.reg));
    }
    else if (waiting) {
        ImGui::Text("FX0A waits for a key (into v%X)", static_cast<unsigned>(state.key_wait.reg));
    }
    else {
        ImGui::TextDisabled("Hold the mouse button on a key to press it");
    }

    ImGui::End();
}

} // namespace c8
