#include "status_bar.h"

#include "app/session.h"
#include "chip8/chip8.h"
#include "debugger/debugger.h"
#include "frontend/rate_meter.h"
#include "frontend/rom_files.h"
#include "frontend/settings.h"
#include "ui/machine_ui.h"

#include <algorithm>
#include <format>
#include <string>

#include <imgui_internal.h> // BeginViewportSideBar

namespace c8 {

namespace {

constexpr auto running_color  = ImVec4{0.45f, 0.90f, 0.45f, 1.0f};
constexpr auto stepping_color = ImVec4{0.45f, 0.80f, 1.00f, 1.0f};
constexpr auto paused_color   = ImVec4{1.00f, 0.80f, 0.30f, 1.0f};
constexpr auto fault_color    = ImVec4{1.00f, 0.40f, 0.40f, 1.0f};

// A message of the log shows this long. It fades out in the last second.
constexpr double message_seconds = 5.0;


// True after a click on the last item. While the mouse is on the item, the hand cursor and a tooltip show that a click does something.
auto clicked(const char* tooltip) -> bool {
    if (!ImGui::IsItemHovered()) {
        return false;
    }
    ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
    ImGui::SetItemTooltip("%s", tooltip);
    return ImGui::IsItemClicked(ImGuiMouseButton_Left);
}


// The text, cut at the end with "..." so it fits in a width
auto fit_text(std::string text, float width) -> std::string {
    if (ImGui::CalcTextSize(text.c_str()).x <= width) {
        return text;
    }

    while (!text.empty() && (ImGui::CalcTextSize((text + "...").c_str()).x > width)) {
        // Remove a whole UTF-8 code point: the continuation bytes, then the first byte
        while (!text.empty() && ((static_cast<unsigned char>(text.back()) & 0xC0u) == 0x80u)) {
            text.pop_back();
        }
        if (!text.empty()) {
            text.pop_back();
        }
    }
    return text + "...";
}


auto draw_run_state(UiContext& context) -> void {
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
    const auto pc = context.chip.state().pc;
    const auto tooltip = std::format("Show the PC (0x{:04X}) in the Code Editor", pc);

    bool show = false;
    ImGui::TextColored(paused_color, "Paused");
    show |= clicked(tooltip.c_str());

    if (!message.empty()) {
        ImGui::TextColored((stop.reason == StopReason::fault) ? fault_color : paused_color, "%s", message.c_str());
        show |= clicked(tooltip.c_str());
    }

    if (show) {
        context.requests.show_in(ShowTarget::code_editor, pc);
    }
}


auto draw_program(const UiContext& context) -> void {
    const auto& session = context.session;
    const auto& name = session.program_name();

    if (name.empty()) {
        ImGui::TextDisabled("No ROM");
        return;
    }

    ImGui::TextUnformatted(name.c_str());

    // A ROM file has a CRC. An assembled program or a state with another program has none.
    if (session.rom_crc() && !session.rom_path().empty()) {
        const auto path = path_to_utf8(session.rom_path());
        ImGui::SetItemTooltip("%s", path.c_str());
    }
}


// The platform and the preset. A click opens a menu that changes them.
auto draw_machine(UiContext& context) -> void {
    const auto& display = context.chip.state().display;
    const auto platform = std::string{to_string(context.chip.variant())};

    ImGui::Text("%s %zux%zu", platform.c_str(), display.width(), display.height());
    if (clicked("Change the platform. A change resets the machine.")) {
        ImGui::OpenPopup("##platforms");
    }
    if (ImGui::BeginPopup("##platforms")) {
        platform_items(context);
        ImGui::EndPopup();
    }
    ImGui::Separator();

    ImGui::TextUnformatted(preset_name(context.chip).c_str());
    if (clicked("Apply a preset: the platform, the quirks, and the instructions per frame")) {
        ImGui::OpenPopup("##presets");
    }
    if (ImGui::BeginPopup("##presets")) {
        preset_items(context);
        ImGui::EndPopup();
    }
}

} // namespace


auto StatusBar::draw(UiContext& context) -> void {
    constexpr auto flags = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_MenuBar;
    const float height = ImGui::GetFrameHeight();

    // The window must end even if BeginViewportSideBar() returns false
    if (ImGui::BeginViewportSideBar("##StatusBar", ImGui::GetMainViewport(), ImGuiDir_Down, height, flags)) {
        if (ImGui::BeginMenuBar()) {
            draw_run_state(context);
            ImGui::Separator();

            draw_program(context);
            ImGui::Separator();

            draw_machine(context);
            ImGui::Separator();

            ImGui::Text("%u IPF", context.chip.instructions_per_frame());
            ImGui::Separator();

            ImGui::Text("Emulated: %.0f IPS, %.0f FPS", context.rate.instructions_per_second, context.rate.frames_per_second);
            ImGui::Separator();

            ImGui::Text("Host: %.0f FPS", ImGui::GetIO().Framerate);

            draw_problem(context);

            ImGui::EndMenuBar();
        }
    }
    ImGui::End();
}


auto StatusBar::draw_problem(UiContext& context) -> void {
    const double now = ImGui::GetTime();

    // The log changes often (a logpoint writes in each frame), so ask for the problem only after a change
    const auto& log = app_log();
    if (log.version() != seen_version) {
        seen_version = log.version();
        if (const auto problem = log.newest_problem(); problem && (problem->sequence != shown_sequence)) {
            shown_sequence = problem->sequence;
            level = problem->level;
            message = problem->message;
            shown_at = now;
        }
    }

    const double age = now - shown_at;
    if (message.empty() || (age >= message_seconds)) {
        return;
    }

    // The message goes to the right end of the bar, and it is cut to fit the space after the other items
    const auto& style = ImGui::GetStyle();
    const float space = ImGui::GetContentRegionAvail().x - (style.ItemSpacing.x * 2.0f);
    if (space < (ImGui::GetFontSize() * 4.0f)) {
        return;
    }

    const auto text = fit_text(message, space);
    const float width = ImGui::CalcTextSize(text.c_str()).x;
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (space - width) + style.ItemSpacing.x);

    const auto color = (level == LogLevel::error) ? fault_color : paused_color;
    const float alpha = std::clamp(static_cast<float>(message_seconds - age), 0.0f, 1.0f);

    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, alpha);
    ImGui::TextColored(color, "%s", text.c_str());
    ImGui::PopStyleVar();

    // The message stays while the mouse is on it
    if (ImGui::IsItemHovered()) {
        shown_at = now;
    }
    const auto tooltip = std::format("{}\nClick to open the Log.", message);
    if (clicked(tooltip.c_str())) {
        context.requests.show_in(ShowTarget::log);
    }
}

} // namespace c8
