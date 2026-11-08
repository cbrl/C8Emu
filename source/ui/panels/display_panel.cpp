#include "display_panel.h"

#include "chip8/chip8.h"
#include "frontend/settings.h"

namespace c8 {

auto DisplayPanel::draw(UiContext& context) -> void {
    if (ImGui::Begin(title(), &open, ImGuiWindowFlags_AlwaysAutoResize)) {
        const auto& display = context.chip.state().display;
        const auto scale = static_cast<float>(context.settings.display_scale);
        const auto size = ImVec2{static_cast<float>(display.width()) * scale, static_cast<float>(display.height()) * scale};
        const auto padding = ImGui::GetStyle().WindowPadding;

        ImGui::BeginChild("Image", {size.x + (padding.x * 2.0f), size.y + (padding.y * 2.0f)}, ImGuiChildFlags_Borders);

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

        ImGui::EndChild();
    }
    ImGui::End();
}

} // namespace c8
