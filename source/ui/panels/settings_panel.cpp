#include "settings_panel.h"

#include "chip8/chip8.h"
#include "debugger/debugger.h"
#include "frontend/settings.h"
#include "ui/command_ui.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <optional>
#include <span>
#include <string>

namespace c8 {

namespace {

auto rgba_to_floats(uint32_t rgba) -> std::array<float, 4> {
    static constexpr float normalize = 1.0f / 255.0f;
    return {
        static_cast<float>((rgba >> 24) & 0xFF) * normalize,
        static_cast<float>((rgba >> 16) & 0xFF) * normalize,
        static_cast<float>((rgba >> 8) & 0xFF) * normalize,
        static_cast<float>(rgba & 0xFF) * normalize,
    };
}

auto floats_to_rgba(std::span<const float, 4> color) -> uint32_t {
    auto channel = [](float value) {
        return static_cast<uint32_t>(std::lround(std::clamp(value, 0.0f, 1.0f) * 255.0f));
    };
    return (channel(color[0]) << 24) | (channel(color[1]) << 16) | (channel(color[2]) << 8) | channel(color[3]);
}

auto color_edit(const char* label, uint32_t& rgba) -> void {
    auto color = rgba_to_floats(rgba);
    if (ImGui::ColorEdit3(label, color.data())) {
        rgba = floats_to_rgba(color);
    }
}

auto section(const char* name) -> void {
    ImGui::Spacing();
    ImGui::SeparatorText(name);
}


// A checkbox for one quirk. Returns true if the user changed it.
auto quirk_checkbox(const char* label, bool& value, const char* tooltip) -> bool {
    const bool changed = ImGui::Checkbox(label, &value);
    ImGui::SetItemTooltip("%s", tooltip);
    return changed;
}


auto draw_preset_combo(Chip8& chip) -> void {
    const auto* current = find_preset(chip.quirks());
    const auto preview = current ? std::string{current->name} : std::string{"Custom"};

    if (ImGui::BeginCombo("Preset", preview.c_str())) {
        for (const auto& preset : presets) {
            const auto name = std::string{preset.name};
            if (ImGui::Selectable(name.c_str(), current == &preset)) {
                chip.apply_preset(preset);
            }
        }
        ImGui::EndCombo();
    }
    ImGui::SetItemTooltip("A preset sets the quirks and the instructions per frame");
}


auto draw_quirks(Chip8& chip) -> void {
    auto quirks = chip.quirks();
    bool changed = false;

    changed |= quirk_checkbox("Shift uses VY", quirks.shift_uses_vy,
        "8XY6/8XYE: shift VY and store the result in VX (COSMAC VIP).\nOff: shift VX in place.");

    static constexpr auto load_store_names = std::array{
        "I += X + 1 (COSMAC VIP)",
        "I += X (CHIP-48)",
        "I unchanged (SUPER-CHIP)",
    };
    auto load_store = static_cast<int>(quirks.load_store);
    if (ImGui::Combo("Load/store", &load_store, load_store_names.data(), static_cast<int>(load_store_names.size()))) {
        quirks.load_store = static_cast<LoadStoreQuirk>(load_store);
        changed = true;
    }
    ImGui::SetItemTooltip("FX55/FX65: the change to the I register");

    changed |= quirk_checkbox("Jump uses VX", quirks.jump_uses_vx,
        "BNNN: jump to XNN + VX (CHIP-48).\nOff: jump to NNN + V0 (COSMAC VIP).");
    changed |= quirk_checkbox("VF reset", quirks.vf_reset,
        "8XY1/8XY2/8XY3: set VF to 0 after the operation (COSMAC VIP).");
    changed |= quirk_checkbox("Wrap sprites", quirks.wrap_sprites,
        "DXYN: wrap the pixels past the screen edge to the other side.\nOff: clip them.");
    changed |= quirk_checkbox("Display wait", quirks.display_wait,
        "DXYN: wait for the next 60 Hz frame after a draw (COSMAC VIP).");
    changed |= quirk_checkbox("Key wait on release", quirks.key_wait_release,
        "FX0A: complete when the key is released (COSMAC VIP).\nOff: complete when the key is pressed.");

    if (changed) {
        chip.set_quirks(quirks);
    }
}


auto draw_speed(Chip8& chip, Settings& settings) -> void {
    // The timers count at 60 Hz, so each frame runs this many instructions
    static constexpr uint32_t ipf_step      = 1;
    static constexpr uint32_t ipf_step_fast = 10;

    auto ipf = chip.instructions_per_frame();
    if (ImGui::InputScalar("Instructions/frame", ImGuiDataType_U32, &ipf, &ipf_step, &ipf_step_fast)) {
        chip.set_instructions_per_frame(ipf);
    }
    ImGui::TextDisabled("%u instructions per second", chip.instructions_per_frame() * Chip8::frame_rate);

    static constexpr auto min_turbo = static_cast<int>(Settings::min_turbo_factor);
    static constexpr auto max_turbo = static_cast<int>(Settings::max_turbo_factor);

    int turbo = static_cast<int>(settings.turbo_factor);
    if (ImGui::SliderInt("Turbo speed", &turbo, min_turbo, max_turbo, "x%d", ImGuiSliderFlags_AlwaysClamp)) {
        settings.turbo_factor = static_cast<uint32_t>(turbo);
    }
    ImGui::SetItemTooltip("Hold Tab to run this many times faster");
}


auto draw_run_control(UiContext& context) -> void {
    command_button(context, "Reset", Command::reset);
    ImGui::SameLine();
    command_button(context, context.debugger.is_running() ? "Pause" : "Run", Command::run_pause);
    ImGui::SameLine();
    command_button(context, "Frame Advance", Command::frame_advance);

    command_button(context, "Step Into", Command::step_into);
    ImGui::SameLine();
    command_button(context, "Step Over", Command::step_over);
    ImGui::SameLine();
    command_button(context, "Step Out", Command::step_out);
}


auto draw_display(Settings& settings) -> void {
    // The host frame rate. The emulation speed does not change with it.
    static constexpr uint32_t frame_limit_step      = 1;
    static constexpr uint32_t frame_limit_step_fast = 10;

    ImGui::Checkbox("Limit Frame Rate", &settings.frame_limit_enabled);
    ImGui::BeginDisabled(!settings.frame_limit_enabled);
    if (ImGui::InputScalar("Max FPS", ImGuiDataType_U32, &settings.frame_limit, &frame_limit_step, &frame_limit_step_fast)) {
        settings.frame_limit = std::clamp(settings.frame_limit, Settings::min_frame_limit, Settings::max_frame_limit);
    }
    ImGui::EndDisabled();

    color_edit("Background Color", settings.background_color);
    color_edit("Foreground Color", settings.foreground_color);
}


auto draw_audio(Settings& settings) -> void {
    int percent = static_cast<int>(std::lround(settings.volume * 100.0f));
    if (ImGui::SliderInt("Volume", &percent, 0, 100, "%d%%", ImGuiSliderFlags_AlwaysClamp)) {
        settings.volume = static_cast<float>(percent) / 100.0f;
    }
    ImGui::Checkbox("Mute", &settings.muted);
}

} // namespace


auto SettingsPanel::draw(UiContext& context) -> void {
    if (ImGui::Begin(title(), &open)) {
        section("Run Control");
        draw_run_control(context);

        section("Emulation");
        draw_emulation(context);

        section("Display");
        draw_display(context.settings);

        section("Audio");
        draw_audio(context.settings);

        section("Keypad");
        ImGui::TextDisabled("1 2 3 4 / Q W E R / A S D F / Z X C V");
    }
    ImGui::End();
}


auto SettingsPanel::draw_emulation(UiContext& context) -> void {
    auto& chip = context.chip;

    draw_preset_combo(chip);
    draw_quirks(chip);

    ImGui::Spacing();
    draw_speed(chip, context.settings);

    ImGui::Spacing();
    bool leave_stops = context.debugger.break_on_leave_program();
    if (ImGui::Checkbox("Break when PC leaves program", &leave_stops)) {
        context.debugger.set_break_on_leave_program(leave_stops);
    }
    ImGui::SetItemTooltip("Pause when the PC goes outside the loaded program");

    draw_rng_seed(context);
}


auto SettingsPanel::draw_rng_seed(UiContext& context) -> void {
    auto& chip = context.chip;

    bool fixed = chip.get_seed().has_value();
    if (fixed) {
        seed = *chip.get_seed();
    }

    if (ImGui::Checkbox("Fixed RNG seed", &fixed)) {
        chip.set_seed(fixed ? std::optional<uint32_t>{seed} : std::nullopt);
    }
    ImGui::SetItemTooltip("CXNN gives the same random numbers after each reset");

    if (fixed) {
        ImGui::SameLine();
        ImGui::SetNextItemWidth(ImGui::GetFontSize() * 6.0f);
        if (ImGui::InputScalar("##seed", ImGuiDataType_U32, &seed, nullptr, nullptr, "%08X", ImGuiInputTextFlags_EnterReturnsTrue)) {
            chip.set_seed(seed);
        }
        ImGui::SetItemTooltip("The seed (hex). Press Enter to use it.");
    }
}

} // namespace c8
