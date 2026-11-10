#include "breakpoint_marker.h"

#include "debugger/debugger.h"

#include <format>

namespace c8 {

namespace {

constexpr ImU32 plain_color     = IM_COL32(230, 60, 60, 255);
constexpr ImU32 condition_color = IM_COL32(240, 150, 40, 255);
constexpr ImU32 logpoint_color  = IM_COL32(80, 160, 255, 255);

} // namespace


auto draw_breakpoint_marker(ImDrawList& draw_list, ImVec2 center, float radius, const Breakpoint* breakpoint, bool hovered) -> void {
    if (breakpoint == nullptr) {
        if (hovered) {
            draw_list.AddCircle(center, radius, plain_color);
        }
        return;
    }

    auto color = plain_color;
    if (breakpoint->is_logpoint()) {
        color = logpoint_color;
    }
    else if (breakpoint->condition.value || (breakpoint->ignore_count > 0)) {
        color = condition_color;
    }

    if (breakpoint->enabled) {
        draw_list.AddCircleFilled(center, radius, color);
    }
    else {
        draw_list.AddCircle(center, radius, color, 0, 1.5f);
    }
}


auto breakpoint_summary(const Breakpoint& breakpoint) -> std::string {
    auto text = std::string{};
    const auto add = [&text](std::string line) {
        text += text.empty() ? "" : "\n";
        text += line;
    };

    if (!breakpoint.enabled) {
        add("Disabled");
    }
    if (breakpoint.condition.is_set()) {
        add(std::format("Condition: {}", breakpoint.condition.text));
        if (!breakpoint.condition.error.empty()) {
            add(std::format("The condition has an error, so the breakpoint always stops. {}", breakpoint.condition.error));
        }
    }
    if (breakpoint.ignore_count > 0) {
        add(std::format("Ignore the first {} hits ({} so far)", breakpoint.ignore_count, breakpoint.hit_count));
    }
    if (breakpoint.log.is_set()) {
        add(std::format("Log: {}", breakpoint.log.text));
        if (!breakpoint.log.error.empty()) {
            add(std::format("The message has an error, so the breakpoint stops. {}", breakpoint.log.error));
        }
    }

    return text;
}

} // namespace c8
