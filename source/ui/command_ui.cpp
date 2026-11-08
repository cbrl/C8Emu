#include "command_ui.h"

#include <string>

namespace c8 {

auto command_menu_item(UiContext& context, const char* label, Command command) -> void {
    const auto keys = shortcut_text(command);
    const bool enabled = is_enabled(command, context.debugger);

    if (ImGui::MenuItem(label, keys.empty() ? nullptr : keys.c_str(), false, enabled)) {
        context.requests.push(command);
    }
}


auto command_button(UiContext& context, const char* label, Command command) -> void {
    ImGui::BeginDisabled(!is_enabled(command, context.debugger));

    if (ImGui::Button(label)) {
        context.requests.push(command);
    }

    if (const auto keys = shortcut_text(command); !keys.empty()) {
        ImGui::SetItemTooltip("%s", keys.c_str());
    }

    ImGui::EndDisabled();
}

} // namespace c8
