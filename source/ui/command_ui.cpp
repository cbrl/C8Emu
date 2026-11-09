#include "command_ui.h"

#include "frontend/settings.h"

#include <string>

namespace c8 {

auto command_menu_item(UiContext& context, const char* label, Command command) -> void {
    const auto keys = shortcut_text(command);
    const bool enabled = is_enabled(command, context.debugger);

    if (ImGui::MenuItem(label, keys.empty() ? nullptr : keys.c_str(), false, enabled)) {
        context.requests.push(command);
    }
}


auto disassemble_menu(UiContext& context, const char* label) -> void {
    if (!ImGui::BeginMenu(label, is_enabled(Command::disassemble, context.debugger))) {
        return;
    }

    auto& mode = context.settings.disassembly_mode;

    const auto item = [&](const char* name, DisassemblyMode value, const char* tooltip) {
        if (ImGui::MenuItem(name, nullptr, mode == value)) {
            mode = value;
            context.requests.push(Command::disassemble);
        }
        ImGui::SetItemTooltip("%s\nOpen ROM uses the checked mode.", tooltip);
    };

    item("Follow Code Flow", DisassemblyMode::flow,
        "Follow the jumps, calls, and skips from the start of the program.\n"
        "Labels mark the targets. The bytes that no path reaches are data.");
    item("Linear", DisassemblyMode::linear,
        "Show each 2-byte word as an instruction, with no labels.\n"
        "Use it if the program runs code that no path reaches.");

    ImGui::EndMenu();
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
