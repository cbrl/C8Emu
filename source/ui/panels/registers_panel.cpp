#include "registers_panel.h"

#include "chip8/chip8.h"
#include "util/strings.h"

#include <array>
#include <format>
#include <string>
#include <string_view>

#include <imgui_stdlib.h>

namespace c8 {

namespace {

constexpr auto edit_flags = ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll;

// An input field as wide as its text. Returns true after Enter.
auto hex_field(const char* id, std::string& text) -> bool {
    ImGui::SetNextItemWidth(ImGui::CalcTextSize(text.c_str()).x);
    return ImGui::InputText(id, &text, edit_flags);
}

// A labeled hex field for a value. Enter writes the new value.
template<typename T>
auto value_field(const char* label, T& value, int digits) -> void {
    auto text = std::format("0x{:0{}X}", value, digits);

    ImGui::PushID(label);
    ImGui::TextUnformatted(label);
    ImGui::SameLine();
    if (hex_field("##value", text)) {
        value = parse_hex<T>(text).value_or(value);
    }
    ImGui::PopID();
}

// A small circle that is lit when `on` is true, then a label
auto indicator(bool on, const char* label) -> void {
    const float size = ImGui::GetTextLineHeight();
    const ImVec2 pos = ImGui::GetCursorScreenPos();
    const auto color = on ? IM_COL32(80, 220, 80, 255) : ImGui::GetColorU32(ImGuiCol_TextDisabled);

    ImGui::GetWindowDrawList()->AddCircleFilled({pos.x + (size * 0.5f), pos.y + (size * 0.5f)}, size * 0.3f, color);
    ImGui::Dummy({size, size});
    ImGui::SameLine();

    if (on) {
        ImGui::TextUnformatted(label);
    }
    else {
        ImGui::TextDisabled("%s", label);
    }
}


auto draw_registers(MachineState& state) -> void {
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2{0, 0});

    for (uint8_t i = 0; i <= 0xF; ++i) {
        const auto label = std::format("v{:X}:", i);
        value_field(label.c_str(), state.v[i], 2);
    }

    ImGui::Separator();
    value_field(" I:", state.i, 4);
    value_field("PC:", state.pc, 4);

    ImGui::Separator();
    value_field("DT:", state.delay_timer, 2);
    ImGui::SameLine();
    ImGui::TextDisabled("(%u)", static_cast<unsigned>(state.delay_timer));

    value_field("ST:", state.sound_timer, 2);
    ImGui::SameLine();
    ImGui::TextDisabled("(%u)", static_cast<unsigned>(state.sound_timer));
    ImGui::SameLine();
    indicator(state.sound_timer > 0, "Sound");

    ImGui::PopStyleVar();
}


// The keys in the layout of the COSMAC VIP keypad. A held key is lit.
auto draw_keypad(const MachineState& state, bool waiting) -> void {
    static constexpr auto layout = std::array<uint8_t, 16>{
        0x1, 0x2, 0x3, 0xC,
        0x4, 0x5, 0x6, 0xD,
        0x7, 0x8, 0x9, 0xE,
        0xA, 0x0, 0xB, 0xF,
    };
    static constexpr auto digits = std::string_view{"0123456789ABCDEF"};

    const float cell = ImGui::GetFrameHeight();
    const float gap = ImGui::GetStyle().ItemInnerSpacing.x;
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    auto* draw_list = ImGui::GetWindowDrawList();

    const auto held_color = ImGui::GetColorU32(ImGuiCol_ButtonActive);
    const auto idle_color = ImGui::GetColorU32(ImGuiCol_FrameBg);
    const auto wait_color = ImGui::GetColorU32(ImGuiCol_PlotHistogram);
    const auto text_color = ImGui::GetColorU32(ImGuiCol_Text);
    const float rounding = ImGui::GetStyle().FrameRounding;

    // FX0A with the release quirk waits for this key to be released
    const bool release_wait = waiting && (state.key_wait.phase == KeyWaitPhase::wait_release);

    for (size_t n = 0; n < layout.size(); ++n) {
        const uint8_t key = layout[n];
        const float x = origin.x + (static_cast<float>(n % 4) * (cell + gap));
        const float y = origin.y + (static_cast<float>(n / 4) * (cell + gap));

        auto color = state.key_down(key) ? held_color : idle_color;
        if (release_wait && (state.key_wait.key == key)) {
            color = wait_color;
        }
        draw_list->AddRectFilled({x, y}, {x + cell, y + cell}, color, rounding);

        const char label[2] = {digits[key], '\0'};
        const ImVec2 text_size = ImGui::CalcTextSize(label);
        draw_list->AddText({x + ((cell - text_size.x) * 0.5f), y + ((cell - text_size.y) * 0.5f)}, text_color, label);
    }

    const float side = (4.0f * cell) + (3.0f * gap);
    ImGui::Dummy({side, side});
}


auto draw_counters(const Chip8& chip) -> void {
    const auto& state = chip.state();

    ImGui::Text("Frame:        %llu", static_cast<unsigned long long>(state.frame_count));
    ImGui::Text("Cycle:        %u / %u", state.cycle_in_frame, chip.instructions_per_frame());
    ImGui::Text("Instructions: %llu", static_cast<unsigned long long>(state.total_cycles));
}

} // namespace


auto RegistersPanel::draw(UiContext& context) -> void {
    if (ImGui::Begin(title(), &open, ImGuiWindowFlags_AlwaysAutoResize)) {
        auto& chip = context.chip;
        auto& state = chip.state();

        ImGui::SeparatorText("Registers");
        draw_registers(state);

        ImGui::SeparatorText("Keypad");
        const bool waiting = chip.waiting_for_key();
        draw_keypad(state, waiting);

        if (waiting && (state.key_wait.phase == KeyWaitPhase::wait_release)) {
            ImGui::Text("FX0A waits for a release (v%X)", static_cast<unsigned>(state.key_wait.reg));
        }
        else if (waiting) {
            ImGui::Text("FX0A waits for a key (v%X)", static_cast<unsigned>(state.key_wait.reg));
        }
        else {
            ImGui::TextDisabled("No key wait");
        }

        ImGui::SeparatorText("Counters");
        draw_counters(chip);
    }
    ImGui::End();
}

} // namespace c8
