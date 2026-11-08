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
        ImGui::Image(context.display_texture, size);
        ImGui::EndChild();
    }
    ImGui::End();
}

} // namespace c8
