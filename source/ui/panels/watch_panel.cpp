#include "watch_panel.h"

#include "chip8/chip8.h"
#include "debugger/debugger.h"
#include "frontend/settings.h"
#include "ui/dock_layout.h"

#include <format>
#include <optional>
#include <string>
#include <vector>

#include <imgui_stdlib.h>

namespace c8 {

auto WatchPanel::draw(UiContext& context) -> void {
    if (!restored) {
        restore(context);
    }

    dock_with_on_first_use("Stack");
    if (ImGui::Begin(title(), &open)) {
        draw_watches(context);
    }
    ImGui::End();

    save(context);
}


auto WatchPanel::restore(const UiContext& context) -> void {
    restored = true;

    const auto& debugger = context.debugger;
    for (const auto& text : context.settings.watches) {
        auto watch = Watch{.edit = text, .symbols_version = debugger.symbols_version()};
        (void)watch.expression.assign(text, debugger.symbols());
        watches.push_back(std::move(watch));
    }
}


auto WatchPanel::save(UiContext& context) const -> void {
    auto texts = std::vector<std::string>{};
    texts.reserve(watches.size());
    for (const auto& watch : watches) {
        texts.push_back(watch.expression.text);
    }

    if (texts != context.settings.watches) {
        context.settings.watches = std::move(texts);
    }
}


auto WatchPanel::draw_watches(UiContext& context) -> void {
    const auto& debugger = context.debugger;
    const auto& symbols = debugger.symbols();
    const auto version = debugger.symbols_version();

    ImGui::SetNextItemWidth(-ImGui::GetFontSize() * 3.0f);
    bool add = ImGui::InputTextWithHint("##new", "For example [i] or v0 + v1", &new_text, ImGuiInputTextFlags_EnterReturnsTrue);
    if (add) {
        ImGui::SetKeyboardFocusHere(-1);
    }
    ImGui::SetItemTooltip("An expression of the debugger. The Breakpoints panel (Break When) shows the syntax.");

    ImGui::SameLine();
    add |= ImGui::Button("Add");

    if (add && (new_text.find_first_not_of(" \t") != std::string::npos)) {
        auto watch = Watch{.edit = new_text, .symbols_version = version};
        (void)watch.expression.assign(new_text, symbols);
        watches.push_back(std::move(watch));
        new_text.clear();
    }

    if (watches.empty()) {
        ImGui::TextDisabled("Add an expression to see its value while the program runs.");
        return;
    }

    const auto& state = context.chip.state();
    auto removed = std::optional<size_t>{};

    constexpr auto table_flags = ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY;
    if (ImGui::BeginTable("##watches", 4, table_flags)) {
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableSetupColumn("Expression", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Hex", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn("Decimal", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn("##remove", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableHeadersRow();

        for (size_t index = 0; index < watches.size(); ++index) {
            auto& watch = watches[index];

            // A new program has new labels
            if (watch.symbols_version != version) {
                watch.expression.compile(symbols);
                watch.symbols_version = version;
            }

            ImGui::PushID(static_cast<int>(index));
            ImGui::TableNextRow();

            ImGui::TableNextColumn();
            ImGui::SetNextItemWidth(-FLT_MIN);
            ImGui::InputText("##text", &watch.edit);
            if (ImGui::IsItemDeactivatedAfterEdit()) {
                (void)watch.expression.assign(watch.edit, symbols);
            }

            ImGui::TableNextColumn();
            if (watch.expression.value) {
                const auto value = watch.expression.value->evaluate(state);

                // The value at the stop before
                const auto* previous = context.previous_stop;
                const auto before = (previous != nullptr) ? std::optional{watch.expression.value->evaluate(*previous)} : std::nullopt;
                const bool changed = before && (*before != value);
                const auto color = changed ? changed_value_color : ImGui::GetStyleColorVec4(ImGuiCol_Text);

                ImGui::TextColored(color, "%s", format_debug_value(value).c_str());
                if (changed) {
                    ImGui::SetItemTooltip("The value at the stop before: %s", format_debug_value(*before).c_str());
                }
                ImGui::TableNextColumn();
                ImGui::TextColored(color, "%lld", static_cast<long long>(value));
            }
            else if (watch.expression.is_set()) {
                ImGui::TextColored({1.0f, 0.4f, 0.4f, 1.0f}, "error");
                ImGui::SetItemTooltip("%s", watch.expression.error.c_str());
                ImGui::TableNextColumn();
            }
            else {
                ImGui::TextDisabled("-");
                ImGui::TableNextColumn();
            }

            ImGui::TableNextColumn();
            if (ImGui::SmallButton("X")) {
                removed = index;
            }
            ImGui::SetItemTooltip("Remove the expression");

            ImGui::PopID();
        }

        ImGui::EndTable();
    }

    if (removed) {
        watches.erase(watches.begin() + static_cast<std::ptrdiff_t>(*removed));
    }
}

} // namespace c8
