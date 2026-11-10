#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <string>

#include "ui/panel.h"

namespace c8 {

/**
 * @class BreakpointsPanel
 *
 * @brief The breakpoints, the watchpoints, and the break condition
 *
 * @details The Breakpoints tab edits the settings of each breakpoint: on or
 *          off, a condition, an ignore count, and a log message. The
 *          Watchpoints tab stops the program before an instruction reads or
 *          writes a range of memory. The Break When tab stops the program
 *          before any instruction while an expression is true.
 *
 *          An address field takes a number, a label, or an expression, for
 *          example `loop+4`.
 */
class BreakpointsPanel final : public Panel {
public:
    [[nodiscard]]
    auto title() const -> const char* override {
        return "Breakpoints";
    }

    auto draw(UiContext& context) -> void override;

private:
    // The text fields of one breakpoint while the user edits them
    struct EditFields {
        std::string condition;
        std::string log;
    };

    auto draw_breakpoints(UiContext& context) -> void;
    auto draw_watchpoints(UiContext& context) -> void;
    auto draw_condition(UiContext& context) -> void;

    // The address of an address field. Sets `error` if the text is not a valid address.
    [[nodiscard]]
    static auto parse_address(const UiContext& context, const std::string& text, std::string& error) -> std::optional<uint16_t>;

    std::string address_text;
    std::string address_error;
    std::map<uint16_t, EditFields> edits;

    std::string watch_address;
    std::string watch_error;
    int watch_length = 1;
    int watch_access = 1;

    std::string condition_text;
    std::string condition_error;
    bool condition_loaded = false;
};

} // namespace c8
