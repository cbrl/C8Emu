#include "machine_ui.h"

#include "chip8/chip8.h"
#include "debugger/debugger.h"
#include "util/log.h"

namespace c8 {

namespace {

auto select_platform(UiContext& context, Variant variant) -> void {
    if (variant == context.chip.variant()) {
        return;
    }

    if (auto changed = context.debugger.set_variant(variant); !changed) {
        log_error("Cannot change the platform: {}", changed.error());
        return;
    }
    log_info("Platform: {}. The machine was reset.", to_string(variant));
}


auto select_preset(UiContext& context, const Preset& preset) -> void {
    const bool resets = preset.variant != context.chip.variant();

    if (auto applied = context.debugger.apply_preset(preset); !applied) {
        log_error("Cannot use the preset \"{}\": {}", preset.name, applied.error());
        return;
    }

    if (resets) {
        log_info("Preset: {}. The platform is now {}, so the machine was reset.", preset.name, to_string(preset.variant));
    }
}


auto platform_tooltip(Variant variant) -> const char* {
    switch (variant) {
        case Variant::chip8:       return "The COSMAC VIP interpreter: 64x32 pixels and 4 KiB of memory";
        case Variant::chip8_hires: return "The two-page COSMAC VIP interpreter: 64x64 pixels.\nThe program starts with 1260.";
        case Variant::schip:       return "SUPER-CHIP 1.1: 128x64 hires mode, scrolling, a big font, and flag registers";
        case Variant::xochip:      return "XO-CHIP: SUPER-CHIP, 64 KiB of memory, two bit planes (4 colors), and sampled audio";
    }
    return "";
}

} // namespace


auto platform_menu(UiContext& context, const char* label) -> void {
    if (!ImGui::BeginMenu(label)) {
        return;
    }

    for (const auto variant : all_variants) {
        const auto name = std::string{to_string(variant)};
        if (ImGui::MenuItem(name.c_str(), nullptr, variant == context.chip.variant())) {
            select_platform(context, variant);
        }
        ImGui::SetItemTooltip("%s", platform_tooltip(variant));
    }

    ImGui::EndMenu();
}


auto preset_menu(UiContext& context, const char* label) -> void {
    if (!ImGui::BeginMenu(label)) {
        return;
    }

    const auto* current = find_preset(context.chip.variant(), context.chip.quirks());

    for (const auto& preset : presets) {
        const auto name = std::string{preset.name};
        const auto platform = std::string{to_string(preset.variant)};

        // The shortcut column shows the platform of the preset
        if (ImGui::MenuItem(name.c_str(), platform.c_str(), current == &preset)) {
            select_preset(context, preset);
        }
    }

    ImGui::EndMenu();
}


auto platform_combo(UiContext& context, const char* label) -> void {
    const auto preview = std::string{to_string(context.chip.variant())};

    if (ImGui::BeginCombo(label, preview.c_str())) {
        for (const auto variant : all_variants) {
            const auto name = std::string{to_string(variant)};
            if (ImGui::Selectable(name.c_str(), variant == context.chip.variant())) {
                select_platform(context, variant);
            }
            ImGui::SetItemTooltip("%s", platform_tooltip(variant));
        }
        ImGui::EndCombo();
    }
    ImGui::SetItemTooltip("The instruction set, the memory, and the display.\nA change resets the machine.");
}


auto preset_combo(UiContext& context, const char* label) -> void {
    const auto* current = find_preset(context.chip.variant(), context.chip.quirks());
    const auto preview = current ? std::string{current->name} : std::string{"Custom"};

    if (ImGui::BeginCombo(label, preview.c_str())) {
        for (const auto& preset : presets) {
            const auto name = std::string{preset.name};
            if (ImGui::Selectable(name.c_str(), current == &preset)) {
                select_preset(context, preset);
            }
            ImGui::SameLine();
            ImGui::TextDisabled("%s", std::string{to_string(preset.variant)}.c_str());
        }
        ImGui::EndCombo();
    }
    ImGui::SetItemTooltip("A preset sets the platform, the quirks, and the instructions per frame");
}


auto preset_name(const Chip8& chip) -> std::string {
    const auto* preset = find_preset(chip.variant(), chip.quirks());
    return preset ? std::string{preset->name} : std::string{"Custom quirks"};
}

} // namespace c8
