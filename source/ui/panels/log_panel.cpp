#include "log_panel.h"

#include <format>
#include <string>

namespace c8 {

namespace {

constexpr auto level_names = "Debug\0Info\0Warning\0Error\0";

auto level_color(LogLevel level) -> ImVec4 {
    switch (level) {
        case LogLevel::debug:   return ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled);
        case LogLevel::info:    return ImGui::GetStyleColorVec4(ImGuiCol_Text);
        case LogLevel::warning: return ImVec4{1.0f, 0.8f, 0.3f, 1.0f};
        case LogLevel::error:   return ImVec4{1.0f, 0.4f, 0.4f, 1.0f};
    }
    return ImGui::GetStyleColorVec4(ImGuiCol_Text);
}

auto format_entry(const LogEntry& entry) -> std::string {
    return std::format("[{:9.3f}] {:<7}  {}", entry.seconds, to_string(entry.level), entry.message);
}

} // namespace


auto LogPanel::draw([[maybe_unused]] UiContext& context) -> void {
    const float font_size = ImGui::GetFontSize();
    ImGui::SetNextWindowSize({font_size * 40.0f, font_size * 15.0f}, ImGuiCond_FirstUseEver);

    if (ImGui::Begin(title(), &open)) {
        // Copy the log only when it changed. Other threads can write to it.
        const auto& log = app_log();
        if (log.version() != copied_version) {
            copied_version = log.version();
            entries = log.entries();
            scroll_to_end = auto_scroll;
        }

        const bool copy = draw_toolbar();

        visible.clear();
        for (const auto& entry : entries) {
            if ((static_cast<int>(entry.level) >= min_level) && filter.PassFilter(entry.message.c_str())) {
                visible.push_back(&entry);
            }
        }

        if (copy) {
            auto text = std::string{};
            for (const auto* entry : visible) {
                text += format_entry(*entry);
                text += '\n';
            }
            ImGui::SetClipboardText(text.c_str());
        }

        ImGui::Separator();
        draw_messages();
    }
    ImGui::End();
}


auto LogPanel::draw_toolbar() -> bool {
    if (ImGui::Button("Clear")) {
        app_log().clear();
    }

    ImGui::SameLine();
    const bool copy = ImGui::Button("Copy");

    ImGui::SameLine();
    ImGui::Checkbox("Auto-scroll", &auto_scroll);

    ImGui::SameLine();
    ImGui::SetNextItemWidth(ImGui::GetFontSize() * 7.0f);
    ImGui::Combo("##level", &min_level, level_names);

    ImGui::SameLine();
    filter.Draw("Filter", ImGui::GetFontSize() * 12.0f);

    return copy;
}


auto LogPanel::draw_messages() -> void {
    if (ImGui::BeginChild("##messages", {0.0f, 0.0f}, ImGuiChildFlags_None, ImGuiWindowFlags_HorizontalScrollbar)) {
        // Follow new messages only while the view is at the bottom
        const bool at_end = ImGui::GetScrollY() >= ImGui::GetScrollMaxY();

        auto clipper = ImGuiListClipper{};
        clipper.Begin(static_cast<int>(visible.size()));
        while (clipper.Step()) {
            for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row) {
                const auto& entry = *visible[static_cast<size_t>(row)];
                const auto line = format_entry(entry);

                ImGui::PushStyleColor(ImGuiCol_Text, level_color(entry.level));
                ImGui::TextUnformatted(line.c_str(), line.c_str() + line.size());
                ImGui::PopStyleColor();
            }
        }
        clipper.End();

        if (scroll_to_end && at_end) {
            ImGui::SetScrollHereY(1.0f);
        }
        scroll_to_end = false;
    }
    ImGui::EndChild();
}

} // namespace c8
