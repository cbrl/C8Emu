#include "watch_panel.h"

#include "chip8/chip8.h"
#include "debugger/debugger.h"
#include "ui/dock_layout.h"

#include <format>
#include <optional>

#include <imgui_stdlib.h>

namespace c8 {

auto WatchPanel::draw(UiContext& context) -> void {
    dock_with_on_first_use("Stack");

    if (!ImGui::Begin(title(), &open)) {
        ImGui::End();
        return;
    }

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
        ImGui::End();
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
                ImGui::TextUnformatted(format_debug_value(value).c_str());
                ImGui::TableNextColumn();
                ImGui::Text("%lld", static_cast<long long>(value));
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

    ImGui::End();
}

} // namespace c8
