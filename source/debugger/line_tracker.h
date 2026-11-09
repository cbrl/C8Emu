#pragma once

#include <cstdint>
#include <optional>
#include <limits>
#include <string>
#include <string_view>
#include <vector>

#include "assembler/source_map.h"

namespace c8 {

/**
 * @class LineTracker
 *
 * @brief Connects the lines of an edited source text to the program that loaded from it
 *
 * @details After a load, each line that wrote bytes has a tag: its span in
 *          the loaded program. When the text changes, the tags move with
 *          their lines. So a breakpoint and the PC show at the correct line
 *          until the next load, also after lines above them change.
 *
 *          The tracker compares each new text with the loaded text:
 *          - A line that is equal to a loaded line keeps the tag of that line.
 *            A shortest line diff finds these pairs.
 *          - Between two equal pairs, a changed line can take the tag of a
 *            changed loaded line. Lines with the same instruction name or
 *            directive are paired first. A line with no statement (a blank
 *            line, a comment, a label alone, or org) gets no tag.
 *
 *          Each result depends only on the loaded text and the new text, so
 *          an undo back to a loaded line also brings back its tag.
 */
class LineTracker {
public:
    /// The marker of a line with no tag
    static constexpr uint32_t none = std::numeric_limits<uint32_t>::max();

    /// Tag the lines of a text that assembled to `map`. The text becomes the loaded text.
    auto load(std::string_view text, SourceMap map) -> void;

    /// Forget the loaded program. No line has a tag after this.
    auto clear() -> void;

    /// Move the tags to the lines of a new version of the text
    auto update(std::string_view text) -> void;

    /// True if a program loaded from the text
    [[nodiscard]]
    auto has_program() const noexcept -> bool {
        return loaded;
    }

    /// True if a program loaded from the text and the text did not change since then
    [[nodiscard]]
    auto matches_loaded() const noexcept -> bool {
        return loaded && unchanged;
    }

    /// The map of the loaded program. Its line numbers are lines of the loaded text.
    [[nodiscard]]
    auto loaded_map() const noexcept -> const SourceMap& {
        return map;
    }

    /// The loaded span of a line of the current text. nullptr if the line has no tag.
    [[nodiscard]]
    auto span_at(uint32_t line) const noexcept -> const SourceSpan*;

    /// The line of the current text whose loaded span holds an address. std::nullopt if no line has it.
    [[nodiscard]]
    auto line_of(uint16_t address) const noexcept -> std::optional<uint32_t>;

    /// The first line at or after `line` that holds an instruction or data. std::nullopt if none follows.
    [[nodiscard]]
    auto statement_line_from(uint32_t line) const noexcept -> std::optional<uint32_t>;

private:
    // A loaded line and a current line are equal: copy the tag and the statement state
    auto keep_line(uint32_t from, uint32_t to) -> void;

    // Give the tag of a loaded line to a line of the current text
    auto move_tag(uint32_t from, uint32_t to) -> void;

    // Pair the changed lines between two equal pairs. The ranges are [begin, end).
    auto pair_changed_lines(
		uint32_t from_begin,
		uint32_t from_end,
		uint32_t to_begin,
		uint32_t to_end,
        const std::vector<std::string_view>& lines) -> void;

    bool loaded = false;
    bool unchanged = false;
    SourceMap map;

    // The loaded text, line by line. For each line: its span index, and the
    // instruction name or directive of its statement (empty if none).
    std::vector<std::string> loaded_lines;
    std::vector<uint32_t> loaded_span_of_line;
    std::vector<std::string> loaded_keys;

    // The current text: the span index of each line, the line of each span,
    // and which lines hold a statement
    std::vector<uint32_t> span_of_line;
    std::vector<uint32_t> line_of_span;
    std::vector<bool> statement_lines;
};

} // namespace c8
