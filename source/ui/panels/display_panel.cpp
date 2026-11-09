#include "display_panel.h"

#include "chip8/chip8.h"
#include "frontend/settings.h"

#include <algorithm>
#include <cmath>

namespace c8 {

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

    ImGui::End();
}

} // namespace c8
