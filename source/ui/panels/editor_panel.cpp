#include "editor_panel.h"

#include "app/source_document.h"
#include "chip8/chip8.h"
#include "debugger/debugger.h"
#include "frontend/rom_files.h"
#include "ui/command_ui.h"
#include "util/log.h"

#include <algorithm>
#include <format>
#include <optional>
#include <string>

namespace c8 {

namespace {

// The Problems table shows this many rows without a scroll
constexpr float problem_rows = 5.0f;

// The width of the breakpoint column, in glyphs
constexpr size_t gutter_glyphs = 2;

constexpr auto error_color   = ImVec4{1.0f, 0.45f, 0.45f, 1.0f};
constexpr auto warning_color = ImVec4{0.95f, 0.75f, 0.25f, 1.0f};

constexpr ImU32 breakpoint_color = IM_COL32(230, 60, 60, 255);
constexpr ImU32 pc_arrow_color   = IM_COL32(255, 200, 0, 255);
constexpr ImU32 pc_outline_color = IM_COL32(60, 45, 0, 255);


// The PC to show in the editor. None while the program runs: the PC then changes too fast to follow.
auto paused_pc(const UiContext& context) -> std::optional<uint16_t> {
    if (!context.debugger.has_program() || context.debugger.is_running()) {
        return std::nullopt;
    }
    return context.chip.state().pc;
}


// Toggle the breakpoint of a line. Log the reason if the line has no address.
auto toggle_line_breakpoint(UiContext& context, size_t line) -> void {
    const auto bytes = context.document.bytes_of_line(line);
    if (!bytes) {
        log_warning("Code Editor line {}: {}", line + 1, bytes.error());
        return;
    }
    (void)context.debugger.toggle_breakpoint_in(bytes->address, bytes->size);
}


// An arrow that points right, in a box at `pos`
auto draw_pc_arrow(ImDrawList& draw_list, ImVec2 pos, ImVec2 size) -> void {
    const float mid_y = pos.y + (size.y * 0.5f);
    const float left = pos.x + (size.x * 0.1f);
    const float neck = pos.x + (size.x * 0.5f);
    const float tip = pos.x + (size.x * 0.95f);
    const float stem = size.y * 0.13f;
    const float head = size.y * 0.32f;

    const ImVec2 outline[] = {
        {left, mid_y - stem}, {neck, mid_y - stem}, {neck, mid_y - head}, {tip, mid_y},
        {neck, mid_y + head}, {neck, mid_y + stem}, {left, mid_y + stem},
    };

    draw_list.AddRectFilled(outline[0], outline[5], pc_arrow_color);
    draw_list.AddTriangleFilled(outline[2], outline[3], outline[4], pc_arrow_color);
    draw_list.AddPolyline(outline, IM_ARRAYSIZE(outline), pc_outline_color, ImDrawFlags_Closed, 1.0f);
}


// The breakpoint column of one line. The editor calls this for each visible line.
auto draw_line_gutter(UiContext& context, const TextEditor::Decorator& decorator, std::optional<uint32_t> pc_line) -> void {
    const auto line = static_cast<uint32_t>(decorator.line);
    const auto* span = context.document.lines().span_at(line);

    const ImVec2 pos = ImGui::GetCursorScreenPos();
    const ImVec2 size = {decorator.width, decorator.height};

    if (ImGui::InvisibleButton("##gutter", size)) {
        toggle_line_breakpoint(context, line);
    }
    const bool hovered = ImGui::IsItemHovered();

    auto& draw_list = *ImGui::GetWindowDrawList();
    const ImVec2 center = {pos.x + (size.x * 0.5f), pos.y + (size.y * 0.5f)};
    const float radius = std::min(size.x, size.y) * 0.3f;

    const bool has_breakpoint = (span != nullptr) && context.debugger.has_breakpoint_in(span->address, span->size);
    if (has_breakpoint) {
        draw_list.AddCircleFilled(center, radius, breakpoint_color);
    }
    else if (hovered) {
        draw_list.AddCircle(center, radius, breakpoint_color);
    }

    const bool at_pc = (pc_line == line);
    if (at_pc) {
        draw_pc_arrow(draw_list, pos, size);
    }

    if (!hovered) {
        return;
    }

    // A line with no bytes toggles the breakpoint of the next line with bytes
    auto tooltip = std::string{};
    if (const auto bytes = context.document.bytes_of_line(line)) {
        const bool exists = context.debugger.has_breakpoint_in(bytes->address, bytes->size);
        tooltip = std::format("{} the breakpoint at 0x{:03X}", exists ? "Remove" : "Add", bytes->address);
        if (bytes->line != line) {
            tooltip += std::format(" (line {})", bytes->line + 1);
        }
    }
    else {
        tooltip = bytes.error();
    }
    if (at_pc) {
        tooltip += std::format("\nThe PC (0x{:03X}) is on this line.", context.chip.state().pc);
    }

    ImGui::SetTooltip("%s", tooltip.c_str());
}


// The menu of a right click on a line number
auto draw_line_menu(UiContext& context, size_t line) -> void {
    auto& debugger = context.debugger;

    const auto bytes = context.document.bytes_of_line(line);
    if (!bytes) {
        ImGui::PushTextWrapPos(ImGui::GetFontSize() * 24.0f);
        ImGui::TextDisabled("%s", bytes.error().c_str());
        ImGui::PopTextWrapPos();
        return;
    }

    ImGui::TextDisabled("Line %u: 0x%03X", static_cast<unsigned>(bytes->line + 1), static_cast<unsigned>(bytes->address));
    ImGui::Separator();

    const bool has_breakpoint = debugger.has_breakpoint_in(bytes->address, bytes->size);
    if (ImGui::MenuItem(has_breakpoint ? "Remove Breakpoint" : "Add Breakpoint")) {
        (void)debugger.toggle_breakpoint_in(bytes->address, bytes->size);
    }

    if (ImGui::MenuItem("Run to Here", nullptr, false, debugger.has_program())) {
        debugger.run_to(bytes->address);
    }
    ImGui::SetItemTooltip("Run until the PC reaches this line");

    if (ImGui::MenuItem("Set PC Here", nullptr, false, debugger.has_program() && !debugger.is_running())) {
        context.chip.state().pc = bytes->address;
    }
    ImGui::SetItemTooltip("The next step or run starts at this line. Pause the program first.");

    if (ImGui::MenuItem("Copy Address")) {
        const auto text = std::format("0x{:03X}", bytes->address);
        ImGui::SetClipboardText(text.c_str());
    }
}

} // namespace


auto EditorPanel::draw(UiContext& context) -> void {
    auto& document = context.document;

    // Before the editor draws, so the markers show in this frame
    const auto pc = paused_pc(context);
    document.update_markers(pc);

    const float font_size = ImGui::GetFontSize();
    ImGui::SetNextWindowSize({font_size * 30.0f, font_size * 30.0f}, ImGuiCond_FirstUseEver);

    if (!ImGui::Begin(title(), &open, ImGuiWindowFlags_MenuBar)) {
        ImGui::End();
        return;
    }

    draw_menu_bar(context);
    draw_banner(context);

    // The editor calls these only inside Render(), so a reference to the context of this frame is safe
    auto& editor = document.editor();
    const auto pc_line = pc ? document.lines().line_of(*pc) : std::nullopt;

    editor.SetLineDecorator(gutter_glyphs, [&context, pc_line](TextEditor::Decorator& decorator) {
        draw_line_gutter(context, decorator, pc_line);
    });
    editor.SetLineNumberContextMenuCallback([&context](TextEditor::PopupData& popup) {
        draw_line_menu(context, popup.pos.line);
    });

    // The editor takes the space above the Problems table and the status line
    const auto& style = ImGui::GetStyle();
    const float line_height = ImGui::GetTextLineHeightWithSpacing();
    const float table_height = (ImGui::GetTextLineHeight() + (style.CellPadding.y * 2.0f)) * problem_rows;
    const float bottom_height = line_height + table_height + line_height + (style.ItemSpacing.y * 2.0f);

    editor.Render("##source", {0.0f, -bottom_height});

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
        disassemble_menu(context, "Disassemble Program");
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

    if (ImGui::BeginMenu("Debug")) {
        auto& debugger = context.debugger;

        const auto line = context.document.editor().GetMainCursorPosition().line;
        const auto bytes = context.document.bytes_of_line(line);
        const auto target = bytes ? std::format("Line {}: 0x{:03X}", bytes->line + 1, bytes->address) : bytes.error();

        command_menu_item(context, "Toggle Breakpoint at Cursor", Command::toggle_breakpoint);
        ImGui::SetItemTooltip("%s", target.c_str());
        command_menu_item(context, "Run to Cursor", Command::run_to_cursor);
        ImGui::SetItemTooltip("%s", target.c_str());

        ImGui::Separator();
        if (ImGui::MenuItem("Remove All Breakpoints", nullptr, false, !debugger.breakpoints().empty())) {
            debugger.clear_breakpoints();
        }

        ImGui::EndMenu();
    }

    ImGui::EndMenuBar();
}


auto EditorPanel::draw_banner(UiContext& context) -> void {
    const auto& lines = context.document.lines();
    if (!lines.has_program() || lines.matches_loaded()) {
        return;
    }

    ImGui::AlignTextToFramePadding();
    ImGui::TextColored(warning_color, "The text changed since the last load.");
    ImGui::SetItemTooltip(
        "The machine runs the program of the last load.\n"
        "The breakpoints and the PC arrow stay on their lines.\n"
        "A new line gets an address at the next load.");

    ImGui::SameLine();
    command_button(context, "Assemble & Load", Command::assemble_and_load);
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

    if (const auto bytes = document.bytes_of_line(cursor.line)) {
        ImGui::Text("0x%03X", static_cast<unsigned>(bytes->address));
        if (bytes->line == cursor.line) {
            ImGui::SetItemTooltip("The program address of the cursor line");
        }
        else {
            ImGui::SetItemTooltip("The program address of line %u, the next line with code", static_cast<unsigned>(bytes->line + 1));
        }
    }
    else {
        ImGui::TextDisabled("No address");
        ImGui::SetItemTooltip("%s", bytes.error().c_str());
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
