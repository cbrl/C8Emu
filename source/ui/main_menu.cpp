#include "main_menu.h"

#include "chip8/chip8.h"

#include <string>

namespace c8 {

auto draw_main_menu(UiContext& context, std::span<const std::unique_ptr<Panel>> panels) -> MainMenuActions {
    auto actions = MainMenuActions{};

    if (!ImGui::BeginMainMenuBar()) {
        return actions;
    }

    if (ImGui::BeginMenu("File")) {
        if (ImGui::MenuItem("Open ROM...")) {
            context.requests.open_rom = true;
        }

        ImGui::Separator();

        if (ImGui::MenuItem("Exit")) {
            context.requests.quit = true;
        }

        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Options")) {
        if (ImGui::BeginMenu("Quirk Preset")) {
            const auto* current = find_preset(context.chip.quirks());

            for (const auto& preset : presets) {
                const auto name = std::string{preset.name};
                if (ImGui::MenuItem(name.c_str(), nullptr, current == &preset)) {
                    context.chip.set_quirks(preset.quirks);
                    context.chip.set_instructions_per_frame(preset.instructions_per_frame);
                }
            }

            ImGui::EndMenu();
        }

        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("View")) {
        // Open or close each panel
        for (const auto& panel : panels) {
            ImGui::MenuItem(panel->title(), nullptr, &panel->open);
        }

        ImGui::Separator();

        if (ImGui::MenuItem("Reset Layout")) {
            actions.reset_layout = true;
        }

        ImGui::EndMenu();
    }

    ImGui::EndMainMenuBar();
    return actions;
}

} // namespace c8
