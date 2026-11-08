#include "settings_panel.h"

#include "app/session.h"
#include "chip8/chip8.h"
#include "debugger/debugger.h"
#include "frontend/rate_meter.h"
#include "frontend/settings.h"

#include <algorithm>
#include <array>
#include <cmath>
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

auto section_break() -> void {
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
}


auto draw_emulation(UiContext& context) -> void {
    auto& chip = context.chip;

    // Emulation speed. The timers count at 60 Hz, so each frame runs this many instructions.
    static constexpr uint32_t ipf_step      = 1;
    static constexpr uint32_t ipf_step_fast = 10;

    auto ipf = chip.instructions_per_frame();
    ImGui::Text("Instructions/frame");
    if (ImGui::InputScalar("##ipf", ImGuiDataType_U32, &ipf, &ipf_step, &ipf_step_fast)) {
        chip.set_instructions_per_frame(ipf);
    }

    const auto target_ips = chip.instructions_per_frame() * Chip8::frame_rate;
    ImGui::TextDisabled("~%u IPS target", target_ips);
    ImGui::TextDisabled("Emulated: %.0f IPS, %.0f FPS", context.rate.instructions_per_second, context.rate.frames_per_second);
    ImGui::Spacing();

    const auto* preset = find_preset(chip.quirks());
    const auto preset_name = preset ? std::string{preset->name} : std::string{"Custom"};
    ImGui::Text("Quirk Preset: %s", preset_name.c_str());

    bool wrap = chip.quirks().wrap_sprites;
    if (ImGui::Checkbox("Wrap Sprites", &wrap)) {
        auto quirks = chip.quirks();
        quirks.wrap_sprites = wrap;
        chip.set_quirks(quirks);
    }
}


auto draw_run_control(UiContext& context) -> void {
    auto& debugger = context.debugger;

    // Reset the system and load the last program again
    if (ImGui::Button("Reset System")) {
        context.session.reset();
    }

    if (debugger.is_running()) {
        if (ImGui::Button("Pause")) {
            debugger.pause();
        }
    }
    else {
        if (ImGui::Button("Resume")) {
            debugger.run();
        }
        ImGui::SameLine();
        if (ImGui::Button("Step")) {
            debugger.step_into();
        }
    }

    // Why execution stopped
    const auto& stop = debugger.last_stop();
    if (const auto message = describe(stop); !message.empty()) {
        if (stop.reason == StopReason::fault) {
            ImGui::TextColored(ImVec4{1.0f, 0.4f, 0.4f, 1.0f}, "%s", message.c_str());
        }
        else {
            ImGui::TextUnformatted(message.c_str());
        }
    }

    if (context.chip.waiting_for_key()) {
        ImGui::Text("Waiting for a key (v%X)", context.chip.state().key_wait.reg);
    }
}


auto draw_display(Settings& settings) -> void {
    static constexpr uint32_t scale_step = 1;
    if (ImGui::InputScalar("Display Scale", ImGuiDataType_U32, &settings.display_scale, &scale_step)) {
        settings.display_scale = std::clamp(settings.display_scale, Settings::min_display_scale, Settings::max_display_scale);
    }

    // The host frame rate. The emulation speed does not change with it.
    static constexpr uint32_t frame_limit_step      = 1;
    static constexpr uint32_t frame_limit_step_fast = 10;

    ImGui::Checkbox("Limit Frame Rate", &settings.frame_limit_enabled);
    ImGui::BeginDisabled(!settings.frame_limit_enabled);
    if (ImGui::InputScalar("Max FPS", ImGuiDataType_U32, &settings.frame_limit, &frame_limit_step, &frame_limit_step_fast)) {
        settings.frame_limit = std::clamp(settings.frame_limit, Settings::min_frame_limit, Settings::max_frame_limit);
    }
    ImGui::EndDisabled();
    ImGui::TextDisabled("Host: %.0f FPS", ImGui::GetIO().Framerate);

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
        ImGui::Text("Settings");
        ImGui::Separator();
        ImGui::Spacing();

        draw_emulation(context);
        ImGui::Spacing();
        draw_run_control(context);

        section_break();
        draw_display(context.settings);

        section_break();
        draw_audio(context.settings);

        section_break();
        ImGui::TextDisabled("Keypad: 1 2 3 4 / Q W E R / A S D F / Z X C V");
    }
    ImGui::End();
}

} // namespace c8
