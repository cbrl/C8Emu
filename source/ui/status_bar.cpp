#include "status_bar.h"

#include "app/session.h"
#include "chip8/chip8.h"
#include "debugger/debugger.h"
#include "frontend/rate_meter.h"
#include "frontend/settings.h"
#include "ui/machine_ui.h"

#include <string>

#include <imgui_internal.h> // BeginViewportSideBar

namespace c8 {

namespace {

constexpr auto running_color  = ImVec4{0.45f, 0.90f, 0.45f, 1.0f};
constexpr auto stepping_color = ImVec4{0.45f, 0.80f, 1.00f, 1.0f};
constexpr auto paused_color   = ImVec4{1.00f, 0.80f, 0.30f, 1.0f};
constexpr auto fault_color    = ImVec4{1.00f, 0.40f, 0.40f, 1.0f};


auto draw_run_state(const UiContext& context) -> void {
    const auto& debugger = context.debugger;

    if (!debugger.has_program()) {
        ImGui::TextDisabled("No program");
        return;
    }

    if (context.rewinding) {
        ImGui::TextColored(stepping_color, "Rewinding");
        return;
    }

    if (debugger.is_stepping()) {
        ImGui::TextColored(stepping_color, "Stepping");
        return;
    }

    if (debugger.is_running()) {
        if (context.turbo_active) {
            ImGui::TextColored(running_color, "Turbo x%u", context.settings.turbo_factor);
        }
        else {
            ImGui::TextColored(running_color, "Running");
        }
        return;
    }

    const auto& stop = debugger.last_stop();
    const auto message = describe(stop);

    ImGui::TextColored(paused_color, "Paused");
    if (!message.empty()) {
        ImGui::TextColored((stop.reason == StopReason::fault) ? fault_color : paused_color, "%s", message.c_str());
    }
}

} // namespace


auto draw_status_bar(const UiContext& context) -> void {
    constexpr auto flags = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_MenuBar;
    const float height = ImGui::GetFrameHeight();

    // The window must end even if BeginViewportSideBar() returns false
    if (ImGui::BeginViewportSideBar("##StatusBar", ImGui::GetMainViewport(), ImGuiDir_Down, height, flags)) {
        if (ImGui::BeginMenuBar()) {
            draw_run_state(context);
            ImGui::Separator();

            const auto& name = context.session.program_name();
            if (name.empty()) {
                ImGui::TextDisabled("No ROM");
            }
            else {
                ImGui::TextUnformatted(name.c_str());
            }
            ImGui::Separator();

            const auto& display = context.chip.state().display;
            const auto platform = std::string{to_string(context.chip.variant())};
            ImGui::Text("%s %zux%zu", platform.c_str(), display.width(), display.height());
            ImGui::Separator();

            ImGui::TextUnformatted(preset_name(context.chip).c_str());
            ImGui::Separator();

            ImGui::Text("%u IPF", context.chip.instructions_per_frame());
            ImGui::Separator();

            ImGui::Text("Emulated: %.0f IPS, %.0f FPS", context.rate.instructions_per_second, context.rate.frames_per_second);
            ImGui::Separator();

            ImGui::Text("Host: %.0f FPS", ImGui::GetIO().Framerate);

            ImGui::EndMenuBar();
        }
    }
    ImGui::End();
}

} // namespace c8
