#include "editor_panel.h"

#include "app/source_document.h"
#include "debugger/debugger.h"
#include "frontend/rom_files.h"
#include "ui/command_ui.h"

#include <string>

namespace c8 {

namespace {

// The Problems table shows this many rows without a scroll
constexpr float problem_rows = 5.0f;

constexpr auto error_color   = ImVec4{1.0f, 0.45f, 0.45f, 1.0f};
constexpr auto warning_color = ImVec4{0.95f, 0.75f, 0.25f, 1.0f};

constexpr auto not_loaded_hint = "The text changed since the last Assemble & Load (Ctrl+B), so the lines have no known addresses";

} // namespace


auto EditorPanel::draw(UiContext& context) -> void {
    auto& document = context.document;

    // Before the editor draws, so the markers show in this frame
    document.update_markers(context.debugger.breakpoints());

    const float font_size = ImGui::GetFontSize();
    ImGui::SetNextWindowSize({font_size * 30.0f, font_size * 30.0f}, ImGuiCond_FirstUseEver);

    if (!ImGui::Begin(title(), &open, ImGuiWindowFlags_MenuBar)) {
        ImGui::End();
        return;
    }

    draw_menu_bar(context);

    // The editor takes the space above the Problems table and the status line
    const auto& style = ImGui::GetStyle();
    const float line_height = ImGui::GetTextLineHeightWithSpacing();
    const float table_height = (ImGui::GetTextLineHeight() + (style.CellPadding.y * 2.0f)) * problem_rows;
    const float bottom_height = line_height + table_height + line_height + (style.ItemSpacing.y * 2.0f);

    document.editor().Render("##source", {0.0f, -bottom_height});

    draw_problems(context, table_height);
    draw_status_line(context);

    ImGui::End();
}


auto EditorPanel::draw_menu_bar(UiContext& context) -> void {
    if (!ImGui::BeginMenuBar()) {
        return;
    }

    if (ImGui::BeginMenu("File")) {
        command_menu_item(context, "New", Command::new_source);
        command_menu_item(context, "Open...", Command::open_source);
        ImGui::Separator();
        command_menu_item(context, "Save", Command::save_source);
        command_menu_item(context, "Save As...", Command::save_source_as);
        ImGui::Separator();
        command_menu_item(context, "Export ROM...", Command::export_rom);
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Build")) {
        command_menu_item(context, "Assemble & Load", Command::assemble_and_load);
        command_menu_item(context, "Check", Command::check_source);
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Breakpoints")) {
        auto& document = context.document;
        auto& debugger = context.debugger;

        const auto line = document.editor().GetMainCursorPosition().line;
        const auto address = document.address_of_line(line);

        if (ImGui::MenuItem("Toggle at Cursor", nullptr, false, address.has_value())) {
            (void)debugger.toggle_breakpoint(*address);
        }
        if (address) {
            ImGui::SetItemTooltip("Address 0x%03X", static_cast<unsigned>(*address));
        }
        else {
            ImGui::SetItemTooltip("%s", document.differs_from_loaded() ? not_loaded_hint : "No code follows the cursor line");
        }

        if (ImGui::MenuItem("Remove All", nullptr, false, !debugger.breakpoints().empty())) {
            debugger.clear_breakpoints();
        }

        ImGui::EndMenu();
    }

    ImGui::EndMenuBar();
}


auto EditorPanel::draw_problems(UiContext& context, float table_height) -> void {
    const auto& result = context.document.last_check();
    const auto& diagnostics = result.diagnostics;

    if (diagnostics.empty()) {
        ImGui::TextDisabled("No problems");
    }
    else {
        const auto errors = result.error_count();
        const auto warnings = result.warning_count();
        ImGui::Text("Problems: %zu %s, %zu %s", errors, (errors == 1) ? "error" : "errors", warnings, (warnings == 1) ? "warning" : "warnings");
    }

    constexpr auto flags = ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY | ImGuiTableFlags_BordersInnerV
                         | ImGuiTableFlags_SizingFixedFit;

    if (!ImGui::BeginTable("##problems", 3, flags, {0.0f, table_height})) {
        return;
    }

    ImGui::TableSetupColumn("Severity");
    ImGui::TableSetupColumn("Line");
    ImGui::TableSetupColumn("Message", ImGuiTableColumnFlags_WidthStretch);

    const Diagnostic* clicked = nullptr;

    auto clipper = ImGuiListClipper{};
    clipper.Begin(static_cast<int>(diagnostics.size()));

    while (clipper.Step()) {
        for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row) {
            const auto& diagnostic = diagnostics[static_cast<size_t>(row)];
            const bool is_error = (diagnostic.severity == Severity::error);

            ImGui::PushID(row);
            ImGui::TableNextRow();

            ImGui::TableNextColumn();
            ImGui::PushStyleColor(ImGuiCol_Text, is_error ? error_color : warning_color);
            if (ImGui::Selectable(is_error ? "Error" : "Warning", false, ImGuiSelectableFlags_SpanAllColumns)) {
                clicked = &diagnostic;
            }
            ImGui::PopStyleColor();

            ImGui::TableNextColumn();
            ImGui::Text("%u", static_cast<unsigned>(diagnostic.line + 1));

            ImGui::TableNextColumn();
            ImGui::TextUnformatted(diagnostic.message.c_str());

            ImGui::PopID();
        }
    }

    ImGui::EndTable();

    if (clicked != nullptr) {
        go_to(context, *clicked);
    }
}


auto EditorPanel::draw_status_line(UiContext& context) -> void {
    auto& document = context.document;
    const auto cursor = document.editor().GetMainCursorPosition();

    ImGui::Text("Ln %zu, Col %zu", cursor.line + 1, cursor.index + 1);

    ImGui::SameLine();
    ImGui::TextDisabled("|");
    ImGui::SameLine();

    if (const auto address = document.address_of_line(cursor.line)) {
        ImGui::Text("0x%03X", static_cast<unsigned>(*address));
        ImGui::SetItemTooltip("The program address of the cursor line");
    }
    else if (document.differs_from_loaded()) {
        ImGui::TextDisabled("Not loaded");
        ImGui::SetItemTooltip("%s", not_loaded_hint);
    }
    else {
        ImGui::TextDisabled("No code");
        ImGui::SetItemTooltip("No code follows the cursor line");
    }

    ImGui::SameLine();
    ImGui::TextDisabled("|");
    ImGui::SameLine();

    const auto name = document.display_name();
    ImGui::Text("%s%s", name.c_str(), document.is_dirty() ? " *" : "");

    if (document.path().empty()) {
        ImGui::SetItemTooltip("The text has no file. Save creates one.");
    }
    else {
        const auto full_path = path_to_utf8(document.path());
        ImGui::SetItemTooltip("%s%s", full_path.c_str(), document.is_dirty() ? "\nThe text has unsaved changes." : "");
    }
}


auto EditorPanel::go_to(UiContext& context, const Diagnostic& diagnostic) -> void {
    auto& editor = context.document.editor();
    const auto position = TextEditor::DocPos{diagnostic.line, diagnostic.column};

    // SetCursor scrolls only as far as needed. Center a line that is out of view.
    editor.SetCursor(position);
    if (!editor.IsDocPosVisible(position)) {
        editor.ScrollToLine(diagnostic.line, TextEditor::Scroll::alignMiddle);
    }
    editor.SetFocus();
}

} // namespace c8
