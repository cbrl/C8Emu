#include "command_ui.h"

#include "frontend/settings.h"

#include <format>
#include <string>
#include <string_view>

namespace c8 {

namespace {

// The label, or the name of the command
auto label_of(Command command, const char* label) -> std::string {
    return (label != nullptr) ? std::string{label} : std::string{command_info(command).name};
}

} // namespace


auto command_menu_item(UiContext& context, Command command, const char* label) -> void {
    const auto& info = command_info(command);
    const auto keys = shortcut_text(command);
    const bool enabled = is_enabled(command, context.debugger);

    if (ImGui::MenuItem(label_of(command, label).c_str(), keys.empty() ? nullptr : keys.c_str(), false, enabled)) {
        context.requests.push(command);
    }

    if (!info.description.empty()) {
        ImGui::SetItemTooltip("%.*s", static_cast<int>(info.description.size()), info.description.data());
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


auto command_button(UiContext& context, Command command, const char* label) -> void {
    const auto& info = command_info(command);

    ImGui::BeginDisabled(!is_enabled(command, context.debugger));

    if (ImGui::Button(label_of(command, label).c_str())) {
        context.requests.push(command);
    }

    // A button with its own label names the command in the tooltip
    const auto keys = shortcut_text(command);
    auto tooltip = std::string{info.description};
    if ((label != nullptr) && (std::string_view{label} != info.name)) {
        tooltip = tooltip.empty() ? std::string{info.name} : std::format("{}: {}", info.name, tooltip);
    }
    if (!keys.empty()) {
        tooltip += tooltip.empty() ? keys : " (" + keys + ")";
    }
    if (!tooltip.empty()) {
        ImGui::SetItemTooltip("%s", tooltip.c_str());
    }

    ImGui::EndDisabled();
}

} // namespace c8
