#pragma once

#include <cstdint>
#include <deque>
#include <string>

#include "assembler/disassembler.h"
#include "ui/panel.h"

namespace c8 {

/**
 * @class TracePanel
 *
 * @brief The newest executed instructions, and the registers before each one
 *
 * @details The filter checks only the entries that are new since the last
 *          frame, so a long trace does not slow the UI. A new filter text
 *          checks the whole trace again, in parts over a few frames.
 */
class TracePanel final : public Panel {
public:
    [[nodiscard]]
    auto title() const -> const char* override {
        return "Trace";
    }

    auto draw(UiContext& context) -> void override;

private:
    // Check the trace entries that are new since the last frame against the filter
    auto update_matches(const UiContext& context) -> void;

    // The text of a row, for the filter and for a copy
    [[nodiscard]]
    auto row_text(const UiContext& context, uint64_t sequence) const -> std::string;

    ImGuiTextFilter filter;
    bool follow = true;

    // The sequence numbers of the entries that pass the filter, oldest first
    std::deque<uint64_t> matches;
    uint64_t checked_until = 0;   // The sequence number after the last checked entry
    std::string checked_filter;   // The filter text of the matches

    // The names of the labels, for the instruction text
    AddressNames names;
    uint64_t names_version = UINT64_MAX;
};

} // namespace c8
