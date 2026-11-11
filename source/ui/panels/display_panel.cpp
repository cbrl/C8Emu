#include "display_panel.h"

#include "chip8/chip8.h"
#include "debugger/debugger.h"
#include "frontend/settings.h"

#include <algorithm>
#include <cmath>
#include <format>
#include <string>

namespace c8 {

namespace {

// Text in the middle of a rectangle, on a dark band. The text wraps at the width of the rectangle.
auto draw_hint(ImDrawList& draw_list, ImVec2 min, ImVec2 max, const char* text, ImU32 text_color) -> void {
    const auto& style = ImGui::GetStyle();
    const float wrap = std::max((max.x - min.x) - (style.WindowPadding.x * 2.0f), ImGui::GetFontSize() * 4.0f);
    const ImVec2 size = ImGui::CalcTextSize(text, nullptr, false, wrap);

    const ImVec2 pos = {min.x + (((max.x - min.x) - size.x) * 0.5f), min.y + (((max.y - min.y) - size.y) * 0.5f)};
    const ImVec2 pad = style.FramePadding;

    draw_list.AddRectFilled({pos.x - pad.x, pos.y - pad.y}, {pos.x + size.x + pad.x, pos.y + size.y + pad.y},
        IM_COL32(0, 0, 0, 180), style.FrameRounding);
    draw_list.AddText(ImGui::GetFont(), ImGui::GetFontSize(), pos, text_color, text, nullptr, wrap);
}

} // namespace


auto DisplayPanel::draw(UiContext& context) -> void {
    // No padding, so the image can use the whole window
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {0.0f, 0.0f});
    const bool visible = ImGui::Begin(title(), &open, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    ImGui::PopStyleVar();

    if (!visible) {
        ImGui::End();
        return;
    }

    const auto& display = context.chip.state().display;
    const auto width = static_cast<float>(display.width());
    const auto height = static_cast<float>(display.height());

    // The largest scale that fits. An integer scale gives each CHIP-8 pixel the same size.
    const ImVec2 available = ImGui::GetContentRegionAvail();
    float scale = std::floor(std::max(std::min(available.x / width, available.y / height), 0.0f));

    const ImVec2 window_min = ImGui::GetCursorScreenPos();
    const ImVec2 window_max = {window_min.x + available.x, window_min.y + available.y};

    const ImVec2 size = {width * scale, height * scale};
    const ImVec2 start = ImGui::GetCursorPos();
    ImGui::SetCursorPos({start.x + ((available.x - size.x) * 0.5f), start.y + ((available.y - size.y) * 0.5f)});

    // The OpenGL3 backend binds a linear sampler object, which overrides the filter of the texture.
    // Select the nearest sampler for the image, then go back to the default.
    const auto& platform_io = ImGui::GetPlatformIO();
    auto* draw_list = ImGui::GetWindowDrawList();
    if (platform_io.DrawCallback_SetSamplerNearest != nullptr) {
        draw_list->AddCallback(platform_io.DrawCallback_SetSamplerNearest);
    }
    ImGui::Image(context.display_texture, size);
    if (platform_io.DrawCallback_SetSamplerLinear != nullptr) {
        draw_list->AddCallback(platform_io.DrawCallback_SetSamplerLinear);
    }

    const auto& debugger = context.debugger;

    if (!debugger.has_program()) {
        const auto keys = shortcut_text(Command::open_rom);
        const auto hint = std::format("Drop a ROM file here, or use File > Open ROM ({})", keys);
        draw_hint(*draw_list, window_min, window_max, hint.c_str(), ImGui::GetColorU32(ImGuiCol_Text));
    }
    else if (debugger.is_running() && context.text_input) {
        // A click on the display takes the focus from the text field, so the keys go to the machine again
        const float band = ImGui::GetFrameHeightWithSpacing() * 2.0f;
        draw_hint(*draw_list, {window_min.x, window_max.y - band}, window_max,
            "A text field has the keyboard. Click the display to play.", ImGui::GetColorU32(ImGuiCol_Text));
    }

    ImGui::End();
}

} // namespace c8
