#include "log_panel.h"

#include "chip8/chip8.h"
#include "ui/address_menu.h"

#include <cctype>
#include <charconv>
#include <format>
#include <optional>
#include <string>
#include <string_view>

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


// The first address in a message: "0x" and 3 or 4 hex digits, for example "0x204" or "0x0204"
auto find_address(std::string_view message) -> std::optional<uint32_t> {
    const auto is_hex = [](char c) { return std::isxdigit(static_cast<unsigned char>(c)) != 0; };

    for (size_t at = message.find("0x"); at != std::string_view::npos; at = message.find("0x", at + 2)) {
        // "0x" must start a word
        if ((at > 0) && (std::isalnum(static_cast<unsigned char>(message[at - 1])) != 0)) {
            continue;
        }

        const size_t first = at + 2;
        size_t end = first;
        while ((end < message.size()) && is_hex(message[end])) {
            ++end;
        }

        const size_t digits = end - first;
        if ((digits < 3) || (digits > 4)) {
            continue;
        }

        uint32_t value = 0;
        (void)std::from_chars(message.data() + first, message.data() + end, value, 16);
        return value;
    }
    return std::nullopt;
}

} // namespace


auto LogPanel::draw(UiContext& context) -> void {
    const float font_size = ImGui::GetFontSize();
    ImGui::SetNextWindowSize({font_size * 40.0f, font_size * 15.0f}, ImGuiCond_FirstUseEver);

    apply_focus_request();
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
        draw_messages(context);
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


auto LogPanel::draw_messages(UiContext& context) -> void {
    static constexpr auto menu = "##message_menu";

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

                if (!ImGui::IsItemHovered()) {
                    continue;
                }

                // The menu keeps a copy of the message, because new messages can move or remove the row
                if (ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
                    menu_entry = entry;
                    ImGui::OpenPopup(menu);
                }
                if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                    const auto address = find_address(entry.message);
                    if (address && (*address < context.chip.state().memory.size())) {
                        context.requests.show_in(ShowTarget::code_editor, *address);
                    }
                }
            }
        }
        clipper.End();

        if (ImGui::BeginPopup(menu)) {
            draw_message_menu(context);
            ImGui::EndPopup();
        }

        if (scroll_to_end && at_end) {
            ImGui::SetScrollHereY(1.0f);
        }
        scroll_to_end = false;
    }
    ImGui::EndChild();
}


auto LogPanel::draw_message_menu(UiContext& context) -> void {
    if (ImGui::MenuItem("Copy Message")) {
        ImGui::SetClipboardText(menu_entry.message.c_str());
    }
    if (ImGui::MenuItem("Copy Line")) {
        ImGui::SetClipboardText(format_entry(menu_entry).c_str());
    }
    ImGui::SetItemTooltip("The message with its time and level");

    if (const auto address = find_address(menu_entry.message)) {
        const auto title = address_with_name(context, *address);
        ImGui::SeparatorText(title.c_str());
        draw_address_menu_items(context, *address, std::nullopt);
    }
}

} // namespace c8
