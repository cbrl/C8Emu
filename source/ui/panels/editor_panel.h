#pragma once

#include "ui/panel.h"

namespace c8 {

struct Diagnostic;

/**
 * @class EditorPanel
 *
 * @brief The code editor, its problems, and its file and build menus
 *
 * @details The text belongs to the SourceDocument of the app, so the text
 *          stays when the panel closes. The app assembles the text shortly
 *          after an edit.
 *
 *          A column after the line numbers shows the breakpoints and, while
 *          the program is paused, an arrow at the line of the PC. A click in
 *          the column toggles a breakpoint. A right click on a line number
 *          opens a menu with more debug actions.
 */
class EditorPanel final : public Panel {
public:
    [[nodiscard]]
    auto title() const -> const char* override {
        return "Code Editor";
    }

    auto draw(UiContext& context) -> void override;

private:
    static auto draw_menu_bar(UiContext& context) -> void;

    // A line above the editor while the text differs from the loaded program
    static auto draw_banner(UiContext& context) -> void;

    static auto draw_problems(UiContext& context, float table_height) -> void;
    static auto draw_status_line(UiContext& context) -> void;

    // Move the cursor to a problem and show its line
    static auto go_to(UiContext& context, const Diagnostic& diagnostic) -> void;
};

} // namespace c8
