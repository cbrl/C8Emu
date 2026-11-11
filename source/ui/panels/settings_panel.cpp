#include "settings_panel.h"

#include "chip8/chip8.h"
#include "debugger/debugger.h"
#include "frontend/settings.h"
#include "ui/command_ui.h"
#include "ui/machine_ui.h"
#include "ui/panels/keypad_panel.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <format>
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
    ImGui::SetItemTooltip("Hold %s to run this many times faster", ImGui::GetKeyName(turbo_key));
}


auto draw_run_control(UiContext& context) -> void {
    command_button(context, Command::reset);
    ImGui::SameLine();
    command_button(context, Command::run_pause, context.debugger.is_running() ? "Pause" : "Run");
    ImGui::SameLine();
    command_button(context, Command::frame_advance);

    command_button(context, Command::step_into);
    ImGui::SameLine();
    command_button(context, Command::step_over);
    ImGui::SameLine();
    command_button(context, Command::step_out);

    command_button(context, Command::step_back);
    ImGui::SameLine();
    command_button(context, Command::frame_back);
}


auto draw_history(UiContext& context) -> void {
    auto& settings = context.settings;

    ImGui::Checkbox("Record History", &settings.history_enabled);
    const auto tooltip = std::format("Record the state at each frame end for Step Back ({}), Frame Back ({}),\n"
                                     "and Rewind (hold {})",
        shortcut_text(Command::step_back), shortcut_text(Command::frame_back), ImGui::GetKeyName(rewind_key));
    ImGui::SetItemTooltip("%s", tooltip.c_str());

    ImGui::BeginDisabled(!settings.history_enabled);
    static constexpr auto min_seconds = static_cast<int>(Settings::min_history_seconds);
    static constexpr auto max_seconds = static_cast<int>(Settings::max_history_seconds);

    int seconds = static_cast<int>(settings.history_seconds);
    if (ImGui::SliderInt("Length", &seconds, min_seconds, max_seconds, "%d s", ImGuiSliderFlags_AlwaysClamp)) {
        settings.history_seconds = static_cast<uint32_t>(seconds);
    }
    ImGui::SetItemTooltip("The oldest frames leave a full history");

    const auto& history = context.debugger.history();
    ImGui::TextDisabled("%zu frames, %zu KiB", history.size(), history.memory_use() / 1024);
    ImGui::EndDisabled();
}


auto draw_display(Settings& settings, Variant variant) -> void {
    // The host frame rate. The emulation speed does not change with it.
    static constexpr uint32_t frame_limit_step      = 1;
    static constexpr uint32_t frame_limit_step_fast = 10;

    ImGui::Checkbox("Limit Frame Rate", &settings.frame_limit_enabled);
    ImGui::BeginDisabled(!settings.frame_limit_enabled);
    if (ImGui::InputScalar("Max FPS", ImGuiDataType_U32, &settings.frame_limit, &frame_limit_step, &frame_limit_step_fast)) {
        settings.frame_limit = std::clamp(settings.frame_limit, Settings::min_frame_limit, Settings::max_frame_limit);
    }
    ImGui::EndDisabled();

    // Only XO-CHIP draws to plane 2, so the other platforms use 2 colors
    auto& palette = settings.palette;
    if (has_xochip(variant)) {
        color_edit("Off", palette[0]);
        color_edit("Plane 1", palette[1]);
        color_edit("Plane 2", palette[2]);
        color_edit("Both Planes", palette[3]);
    }
    else {
        color_edit("Background Color", palette[0]);
        color_edit("Foreground Color", palette[1]);
    }
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

        section("History");
        draw_history(context);

        section("Display");
        draw_display(context.settings, context.chip.variant());

        section("Audio");
        draw_audio(context.settings);

        section("Keypad");
        draw_keypad_map(context);
    }
    ImGui::End();
}


auto SettingsPanel::draw_emulation(UiContext& context) -> void {
    auto& chip = context.chip;

    platform_combo(context, "Platform");
    preset_combo(context, "Preset");
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

    // A new seed changes the random state now, so the history starts again
    if (ImGui::Checkbox("Fixed RNG seed", &fixed)) {
        chip.set_seed(fixed ? std::optional<uint32_t>{seed} : std::nullopt);
        context.requests.state_edited = true;
    }
    ImGui::SetItemTooltip("CXNN gives the same random numbers after each reset");

    if (fixed) {
        ImGui::SameLine();
        ImGui::SetNextItemWidth(ImGui::GetFontSize() * 6.0f);
        if (ImGui::InputScalar("##seed", ImGuiDataType_U32, &seed, nullptr, nullptr, "%08X", ImGuiInputTextFlags_EnterReturnsTrue)) {
            chip.set_seed(seed);
            context.requests.state_edited = true;
        }
        ImGui::SetItemTooltip("The seed (hex). Press Enter to use it.");
    }
}

} // namespace c8
