#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>

#include <imgui-color-text-edit/TextEditor.h>

#include "ui/panel.h"

namespace c8 {

struct Diagnostic;
struct Label;

/**
 * @class EditorPanel
 *
 * @brief The code editor, its problems, and its menus
 *
 * @details The text belongs to the SourceDocument of the app, so the text
 *          stays when the panel closes. The app assembles the text shortly
 *          after an edit.
 *
 *          A column after the line numbers shows the breakpoints and, while
 *          the program is paused, an arrow at the line of the PC. A click in
 *          the column toggles a breakpoint. A right click on a line number
 *          opens a menu with more debug actions.
 *
 *          When the mouse stays on a word, a help window shows the problems
 *          and the information of the word (see draw_source_help). The help
 *          closes while the user types, and it opens again after the mouse
 *          moves. The editor offers completions while the user types, and
 *          F12 or the right-click menu goes to the definition of a name.
 *          Go to Line (Ctrl+Shift+G, or a click on the cursor position)
 *          moves the cursor to a line number.
 */
class EditorPanel final : public Panel {
public:
    [[nodiscard]]
    auto title() const -> const char* override {
        return "Code Editor";
    }

    auto draw(UiContext& context) -> void override;

    /// Bring the panel to the front, put the cursor at the start of the code of a line, and focus the editor
    auto show_line(uint32_t line) -> void {
        bring_to_front();
        pending_line = line;
    }

private:
    // Set the parts of the editor that need the context only one time
    auto configure(UiContext& context) -> void;

    auto draw_menu_bar(UiContext& context) -> void;
    auto draw_edit_menu(UiContext& context) -> void;
    static auto draw_view_menu(UiContext& context) -> void;

    // Give the view options of the settings (line numbers, whitespace, mini map) to the editor
    static auto apply_view_settings(UiContext& context) -> void;

    // A line above the editor while the text differs from the loaded program
    static auto draw_banner(UiContext& context) -> void;

    static auto draw_problems(UiContext& context, float table_height) -> void;

    // The cursor position, the address, and the file. Returns true after a click on the cursor position.
    static auto draw_status_line(UiContext& context) -> bool;

    // The popup of Go to Line. go_to_line_requested opens it.
    auto draw_go_to_line(UiContext& context) -> void;

    // Give the editor the help callback after the mouse stays on one word. Take it away while the user types.
    auto update_help(UiContext& context) -> void;

    // The contents of the help window. The editor calls this while its help window is open.
    auto draw_help(UiContext& context) -> void;

    // The menu of a right click on the text
    static auto draw_text_menu(UiContext& context, const TextEditor::PopupData& popup) -> void;

    // Go to the definition of the name at the cursor
    static auto go_to_cursor_definition(UiContext& context) -> void;

    // Move the cursor to a place and show its line
    static auto go_to(UiContext& context, uint32_t line, uint32_t column) -> void;
    static auto go_to(UiContext& context, const Diagnostic& diagnostic) -> void;
    static auto go_to(UiContext& context, const Label& label) -> void;

    TextEditor::AutoCompleteConfig completion;
    bool configured = false;

    // The line of show_line(). The cursor goes to it when the window is visible.
    std::optional<uint32_t> pending_line;

    // Go to Line opens in the next frame, and the text of its field
    bool go_to_line_requested = false;
    std::string go_to_line_text;

    // The help shows only after a move of the mouse, and not while another popup is open
    bool help_enabled = false;

    // The mouse was on the editor in the last frame
    bool editor_hovered = false;

    // The word under the mouse (line, column), and when the mouse came to it
    std::optional<std::pair<size_t, uint32_t>> help_word;
    double help_start = 0.0;
};

} // namespace c8
