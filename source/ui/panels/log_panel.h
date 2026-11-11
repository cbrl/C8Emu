#pragma once

#include <cstdint>
#include <limits>
#include <vector>

#include "ui/panel.h"
#include "util/log.h"

namespace c8 {

/**
 * @class LogPanel
 *
 * @brief The messages of the application log
 *
 * @details The panel filters by level and by text. It can copy the visible
 *          messages and clear the log. With auto-scroll on, it follows new
 *          messages while the view is at the bottom.
 *
 *          A right click on a message opens a menu that copies it. If the
 *          message has an address (for example "0x0204"), the menu also has
 *          the address menu, and a double click shows the address in the
 *          Code Editor.
 */
class LogPanel final : public Panel {
public:
    [[nodiscard]]
    auto title() const -> const char* override {
        return "Log";
    }

    auto draw(UiContext& context) -> void override;

private:
    auto draw_toolbar() -> bool;
    auto draw_messages(UiContext& context) -> void;

    // The menu of a right click on a message
    auto draw_message_menu(UiContext& context) -> void;

    // The message of the open menu
    LogEntry menu_entry;

    // A copy of the log, updated when the log changes
    std::vector<LogEntry> entries;
    uint64_t copied_version = std::numeric_limits<uint64_t>::max();

    // The messages that pass the filters
    std::vector<const LogEntry*> visible;

    int min_level = static_cast<int>(LogLevel::info);
    ImGuiTextFilter filter;
    bool auto_scroll = true;
    bool scroll_to_end = false;
};

} // namespace c8
