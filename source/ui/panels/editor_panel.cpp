#include "editor_panel.h"

#include "app/source_document.h"
#include "chip8/chip8.h"
#include "debugger/debugger.h"
#include "frontend/rom_files.h"
#include "frontend/settings.h"
#include "ui/address_menu.h"
#include "ui/breakpoint_marker.h"
#include "ui/command_ui.h"
#include "ui/editor_assist.h"
#include "util/log.h"

#include <algorithm>
#include <charconv>
#include <format>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>

#include <imgui_internal.h> // ImGuiKey_Keyboard_BEGIN, GetKeyChordName
#include <imgui_stdlib.h>

namespace c8 {

namespace {

// The Problems table shows this many rows without a scroll
constexpr float problem_rows = 5.0f;

// The width of the breakpoint column, in glyphs
constexpr size_t gutter_glyphs = 2;

constexpr auto error_color   = ImVec4{1.0f, 0.45f, 0.45f, 1.0f};
constexpr auto warning_color = ImVec4{0.95f, 0.75f, 0.25f, 1.0f};

// The editor uses Ctrl+G for Find Next
constexpr auto go_to_line_chord = ImGuiKeyChord{ImGuiMod_Ctrl | ImGuiMod_Shift | ImGuiKey_G};

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

    const auto* breakpoint = (span != nullptr) ? context.debugger.first_breakpoint_in(span->address, span->size) : nullptr;
    draw_breakpoint_marker(draw_list, center, radius, breakpoint, hovered);

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
        const auto* existing = context.debugger.first_breakpoint_in(bytes->address, bytes->size);
        tooltip = std::format("{} the breakpoint at 0x{:03X}", (existing != nullptr) ? "Remove" : "Add", bytes->address);
        if (bytes->line != line) {
            tooltip += std::format(" (line {})", bytes->line + 1);
        }
        if (existing != nullptr) {
            if (const auto summary = breakpoint_summary(*existing); !summary.empty()) {
                tooltip += '\n';
                tooltip += summary;
            }
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
    const auto bytes = context.document.bytes_of_line(line);
    if (!bytes) {
        ImGui::PushTextWrapPos(ImGui::GetFontSize() * 24.0f);
        ImGui::TextDisabled("%s", bytes.error().c_str());
        ImGui::PopTextWrapPos();
        return;
    }

    ImGui::TextDisabled("Line %u: 0x%03X", static_cast<unsigned>(bytes->line + 1), static_cast<unsigned>(bytes->address));
    ImGui::Separator();

    draw_address_menu_items(context, bytes->address, ShowTarget::code_editor, bytes->size);
}


// The column of the first glyph of a line that is not a space
auto code_column(std::string_view text) -> uint32_t {
    const auto column = text.find_first_not_of(" \t");
    return (column == std::string_view::npos) ? 0 : static_cast<uint32_t>(column);
}

} // namespace


auto EditorPanel::draw(UiContext& context) -> void {
    auto& document = context.document;

    if (!configured) {
        configure(context);
    }
    apply_focus_request();

    // Before the editor draws, so the markers show in this frame
    const auto pc = paused_pc(context);
    document.update_view(pc);
    apply_view_settings(context);

    const float font_size = ImGui::GetFontSize();
    ImGui::SetNextWindowSize({font_size * 30.0f, font_size * 30.0f}, ImGuiCond_FirstUseEver);

    if (!ImGui::Begin(title(), &open, ImGuiWindowFlags_MenuBar)) {
        ImGui::End();
        return;
    }

    draw_menu_bar(context);
    draw_banner(context);

    // The cursor and the scroll take effect when the editor renders below
    if (pending_line) {
        const auto text = document.editor().GetLineText(*pending_line);
        go_to(context, *pending_line, code_column(text));
        pending_line.reset();
    }

    // The editor calls these only inside Render(), so a reference to the context of this frame is safe
    auto& editor = document.editor();
    const auto pc_line = pc ? document.lines().line_of(*pc) : std::nullopt;

    editor.SetLineDecorator(gutter_glyphs, [&context, pc_line](TextEditor::Decorator& decorator) {
        draw_line_gutter(context, decorator, pc_line);
    });
    editor.SetLineNumberContextMenuCallback([&context](TextEditor::PopupData& popup) {
        draw_line_menu(context, popup.pos.line);
    });
    editor.SetTextContextMenuCallback([&context](TextEditor::PopupData& popup) {
        draw_text_menu(context, popup);
    });
    update_help(context);

    // The editor takes the space above the Problems table and the status line
    const auto& style = ImGui::GetStyle();
    const float line_height = ImGui::GetTextLineHeightWithSpacing();
    const float table_height = (ImGui::GetTextLineHeight() + (style.CellPadding.y * 2.0f)) * problem_rows;
    const float bottom_height = line_height + table_height + line_height + (style.ItemSpacing.y * 2.0f);

    editor.Render("##source", {0.0f, -bottom_height});

    // The editor is a child window. The help checks this in the next frame.
    editor_hovered = ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenBlockedByPopup | ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);

    // The default route is the focused window and its parents, so F12 works while the focus is in this panel
    if (ImGui::Shortcut(ImGuiKey_F12)) {
        go_to_cursor_definition(context);
    }
    if (ImGui::Shortcut(go_to_line_chord)) {
        go_to_line_requested = true;
    }

    draw_problems(context, table_height);
    if (draw_status_line(context)) {
        go_to_line_requested = true;
    }
    draw_go_to_line(context);

    ImGui::End();
}


auto EditorPanel::configure(UiContext& context) -> void {
    configured = true;

    // The document lives as long as the app, so the completion can keep a reference to it
    auto& document = context.document;

    completion.callback = [&document](TextEditor::AutoCompleteState& state) {
        state.suggestions.clear();
        if (state.inComment) {
            return;
        }

        const auto line = document.editor().GetLineText(state.searchTermEnd.line);
        state.suggestions = source_completions(
            line,
            static_cast<uint32_t>(state.searchTermStart.index),
            static_cast<uint32_t>(state.searchTermEnd.index),
            document.last_check());
    };

    document.editor().SetAutoCompleteConfig(&completion);
}


auto EditorPanel::draw_menu_bar(UiContext& context) -> void {
    if (!ImGui::BeginMenuBar()) {
        return;
    }

    if (ImGui::BeginMenu("File")) {
        command_menu_item(context, Command::new_source, "New");
        command_menu_item(context, Command::open_source, "Open...");
        disassemble_menu(context, "Disassemble Program");
        ImGui::Separator();
        command_menu_item(context, Command::save_source, "Save");
        command_menu_item(context, Command::save_source_as, "Save As...");
        ImGui::Separator();
        command_menu_item(context, Command::export_rom);
        ImGui::EndMenu();
    }

    draw_edit_menu(context);

    if (ImGui::BeginMenu("Build")) {
        command_menu_item(context, Command::assemble_and_load);
        command_menu_item(context, Command::check_source, "Check");
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Debug")) {
        auto& debugger = context.debugger;

        const auto line = context.document.editor().GetMainCursorPosition().line;
        const auto bytes = context.document.bytes_of_line(line);
        const auto target = bytes ? std::format("Line {}: 0x{:03X}", bytes->line + 1, bytes->address) : bytes.error();

        command_menu_item(context, Command::toggle_breakpoint, "Toggle Breakpoint at Cursor");
        ImGui::SetItemTooltip("%s", target.c_str());
        command_menu_item(context, Command::run_to_cursor);
        ImGui::SetItemTooltip("%s", target.c_str());

        ImGui::Separator();
        if (ImGui::MenuItem("Remove All Breakpoints", nullptr, false, !debugger.breakpoints().empty())) {
            debugger.clear_breakpoints();
        }

        ImGui::EndMenu();
    }

    draw_view_menu(context);

    ImGui::EndMenuBar();
}


auto EditorPanel::draw_edit_menu(UiContext& context) -> void {
    if (!ImGui::BeginMenu("Edit")) {
        return;
    }

    auto& editor = context.document.editor();

    if (ImGui::MenuItem("Undo", "Ctrl+Z", false, editor.CanUndo())) {
        editor.Undo();
    }
    if (ImGui::MenuItem("Redo", "Ctrl+Y", false, editor.CanRedo())) {
        editor.Redo();
    }

    // Without a selection, Cut and Copy take the whole line
    ImGui::Separator();
    if (ImGui::MenuItem("Cut", "Ctrl+X")) {
        editor.Cut();
    }
    if (ImGui::MenuItem("Copy", "Ctrl+C")) {
        editor.Copy();
    }
    if (ImGui::MenuItem("Paste", "Ctrl+V")) {
        editor.Paste();
    }

    ImGui::Separator();
    if (ImGui::MenuItem("Find and Replace", "Ctrl+F")) {
        editor.OpenFindReplaceWindow();
    }
    if (ImGui::MenuItem("Toggle Comment", "Ctrl+/")) {
        editor.ToggleComments();
    }

    ImGui::Separator();
    if (ImGui::MenuItem("Go to Definition", "F12")) {
        go_to_cursor_definition(context);
    }
    ImGui::SetItemTooltip("Go to the line that defines the label or the constant at the cursor");

    const auto go_to_line_keys = std::string{ImGui::GetKeyChordName(go_to_line_chord)};
    if (ImGui::MenuItem("Go to Line...", go_to_line_keys.c_str())) {
        go_to_line_requested = true;
    }

    ImGui::EndMenu();
}


auto EditorPanel::draw_view_menu(UiContext& context) -> void {
    if (!ImGui::BeginMenu("View")) {
        return;
    }

    // The next frame gives the changes to the editor
    auto& settings = context.settings;
    ImGui::MenuItem("Line Numbers", nullptr, &settings.editor_line_numbers);
    ImGui::MenuItem("Show Whitespace", nullptr, &settings.editor_whitespace);
    ImGui::MenuItem("Mini Map", nullptr, &settings.editor_mini_map);

    ImGui::EndMenu();
}


auto EditorPanel::apply_view_settings(UiContext& context) -> void {
    const auto& settings = context.settings;
    auto& editor = context.document.editor();

    if (editor.IsShowLineNumbersEnabled() != settings.editor_line_numbers) {
        editor.SetShowLineNumbersEnabled(settings.editor_line_numbers);
    }
    if (editor.IsShowWhitespacesEnabled() != settings.editor_whitespace) {
        editor.SetShowWhitespacesEnabled(settings.editor_whitespace);
    }
    if (editor.IsShowMiniMapEnabled() != settings.editor_mini_map) {
        editor.SetShowMiniMapEnabled(settings.editor_mini_map);
    }
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
    command_button(context, Command::assemble_and_load);
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


auto EditorPanel::draw_status_line(UiContext& context) -> bool {
    auto& document = context.document;
    const auto cursor = document.editor().GetMainCursorPosition();

    ImGui::Text("Ln %zu, Col %zu", cursor.line + 1, cursor.index + 1);
    bool go_to_line = false;
    if (ImGui::IsItemHovered()) {
        ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
        ImGui::SetItemTooltip("Go to Line (%s)", ImGui::GetKeyChordName(go_to_line_chord));
        go_to_line = ImGui::IsItemClicked(ImGuiMouseButton_Left);
    }

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

    return go_to_line;
}


auto EditorPanel::draw_go_to_line(UiContext& context) -> void {
    static constexpr auto popup = "Go to Line";
    auto& editor = context.document.editor();

    if (go_to_line_requested) {
        go_to_line_requested = false;
        go_to_line_text.clear();
        ImGui::OpenPopup(popup);

        // At the top of the panel, in the middle
        const ImVec2 pos = ImGui::GetWindowPos();
        ImGui::SetNextWindowPos({pos.x + (ImGui::GetWindowWidth() * 0.5f), pos.y + (ImGui::GetFrameHeight() * 2.0f)},
            ImGuiCond_Always, {0.5f, 0.0f});
    }

    if (!ImGui::BeginPopup(popup)) {
        return;
    }

    const size_t count = editor.GetLineCount();
    ImGui::Text("Go to line (1 to %zu)", count);

    if (ImGui::IsWindowAppearing()) {
        ImGui::SetKeyboardFocusHere();
    }
    ImGui::SetNextItemWidth(ImGui::GetFontSize() * 12.0f);
    constexpr auto flags = ImGuiInputTextFlags_CharsDecimal | ImGuiInputTextFlags_EnterReturnsTrue;
    if (ImGui::InputTextWithHint("##line", "Line number", &go_to_line_text, flags)) {
        size_t number = 0;
        const auto* begin = go_to_line_text.data();
        const auto* end = begin + go_to_line_text.size();
        if ((std::from_chars(begin, end, number).ec == std::errc{}) && (count > 0)) {
            const auto line = static_cast<uint32_t>(std::clamp<size_t>(number, 1, count) - 1);
            go_to(context, line, code_column(editor.GetLineText(line)));
        }
        ImGui::CloseCurrentPopup();
    }
    else if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
        ImGui::CloseCurrentPopup();
    }

    ImGui::EndPopup();
}


auto EditorPanel::update_help(UiContext& context) -> void {
    const auto& io = ImGui::GetIO();

    bool typed = !io.InputQueueCharacters.empty();
    for (int key = ImGuiKey_Keyboard_BEGIN; !typed && (key < ImGuiKey_Keyboard_END); ++key) {
        typed = ImGui::IsKeyPressed(static_cast<ImGuiKey>(key), false);
    }
    const bool moved = (io.MouseDelta.x != 0.0f) || (io.MouseDelta.y != 0.0f);

    // The help and the completion list are popups at the same level. When one opens, the other closes. So the
    // help stays off while the user types, and it does not come back while another popup is open.
    if (typed) {
        help_enabled = false;
    }
    else if (moved && !help_enabled && !ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel)) {
        help_enabled = true;
    }

    // The word under the mouse. The positions of the editor are from its last frame.
    auto& editor = context.document.editor();
    const auto mouse = ImGui::GetMousePos();

    auto word_key = std::optional<std::pair<size_t, uint32_t>>{};
    if (help_enabled && editor_hovered && editor.IsMousePosOverGlyph(mouse)) {
        const auto position = editor.GetDocPosAtMousePos(mouse);
        const auto text = editor.GetLineText(position.line);
        if (const auto word = source_word_at(text, static_cast<uint32_t>(position.index))) {
            word_key = std::pair{position.line, word->column};
        }
    }

    if (word_key != help_word) {
        help_word = word_key;
        help_start = ImGui::GetTime();
    }

    // The editor opens its help window only while it has the callback. Without the callback, it closes the window.
    // So the window opens only after the mouse stays on one word, and it shows its contents at once.
    const bool waited = (ImGui::GetTime() - help_start) >= static_cast<double>(ImGui::GetStyle().HoverDelayNormal);
    if (help_word && waited) {
        editor.SetTextHoverCallback([this, &context](TextEditor::PopupData& /*popup*/) { draw_help(context); });
    }
    else {
        editor.ClearTextHoverCallback();
    }
}


auto EditorPanel::draw_help(UiContext& context) -> void {
    auto& editor = context.document.editor();
    const auto mouse = ImGui::GetMousePos();

    if (!editor_hovered || !editor.IsMousePosOverGlyph(mouse)) {
        ImGui::CloseCurrentPopup();
        return;
    }

    // The word points into the text of the line, so the text must live until the help is drawn
    const auto position = editor.GetDocPosAtMousePos(mouse);
    const auto line = static_cast<uint32_t>(position.line);
    const auto text = editor.GetLineText(position.line);
    const auto word = source_word_at(text, static_cast<uint32_t>(position.index));

    if (!word || !draw_source_help(context, line, *word)) {
        ImGui::CloseCurrentPopup();
    }
}


auto EditorPanel::draw_text_menu(UiContext& context, const TextEditor::PopupData& popup) -> void {
    auto& document = context.document;
    auto& editor = document.editor();

    const auto text = editor.GetLineText(popup.pos.line);
    if (const auto* label = definition_at(document, text, static_cast<uint32_t>(popup.pos.index))) {
        const auto item = std::format("Go to Definition of {}", label->name);
        if (ImGui::MenuItem(item.c_str())) {
            go_to(context, *label);
        }
        ImGui::Separator();
    }

    if (ImGui::MenuItem("Cut", "Ctrl+X")) {
        editor.Cut();
    }
    if (ImGui::MenuItem("Copy", "Ctrl+C")) {
        editor.Copy();
    }
    if (ImGui::MenuItem("Paste", "Ctrl+V")) {
        editor.Paste();
    }
    if (ImGui::MenuItem("Toggle Comment", "Ctrl+/")) {
        editor.ToggleComments();
    }

    ImGui::Separator();
    draw_line_menu(context, popup.pos.line);
}


auto EditorPanel::go_to_cursor_definition(UiContext& context) -> void {
    auto& document = context.document;
    auto& editor = document.editor();

    const auto cursor = editor.GetMainCursorPosition();
    const auto text = editor.GetLineText(cursor.line);
    const auto column = static_cast<uint32_t>(cursor.index);

    // The cursor can be just after the name
    const auto* label = definition_at(document, text, column);
    if ((label == nullptr) && (column > 0)) {
        label = definition_at(document, text, column - 1);
    }

    if (label == nullptr) {
        log_info("Go to Definition: line {} has no label or constant at the cursor", cursor.line + 1);
        return;
    }
    go_to(context, *label);
}


auto EditorPanel::go_to(UiContext& context, uint32_t line, uint32_t column) -> void {
    auto& editor = context.document.editor();
    const auto position = TextEditor::DocPos{line, column};

    // SetCursor scrolls only as far as needed. Center a line that is out of view.
    editor.SetCursor(position);
    if (!editor.IsDocPosVisible(position)) {
        editor.ScrollToLine(line, TextEditor::Scroll::alignMiddle);
    }
    editor.SetFocus();
}


auto EditorPanel::go_to(UiContext& context, const Diagnostic& diagnostic) -> void {
    go_to(context, diagnostic.line, diagnostic.column);
}


auto EditorPanel::go_to(UiContext& context, const Label& label) -> void {
    const auto text = context.document.editor().GetLineText(label.line);
    go_to(context, label.line, definition_column(text, label));
}

} // namespace c8
