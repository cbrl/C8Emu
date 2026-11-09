#pragma once

#include <cstdint>
#include <set>
#include <vector>

namespace c8 {

class LineTracker;
class SourceMap;

/// The breakpoints after a new program assembled from the edited text
struct RemappedBreakpoints {
    std::set<uint16_t>    breakpoints; ///< The breakpoints at their new addresses
    std::vector<uint16_t> dropped;     ///< The old addresses of the breakpoints whose lines are gone or write no bytes now
};

/**
 * @brief Move the breakpoints of the loaded program to a new program
 *
 * @details A breakpoint in a span of the loaded program moves with the line of
 *          that span: it gets the same offset in the new span of the line. If
 *          the offset is outside the new span, the breakpoint moves to the
 *          start of the span. A breakpoint is dropped if its line was deleted
 *          or writes no bytes now. A breakpoint outside the spans of the loaded
 *          program does not change.
 *
 * @param[in] breakpoints  The breakpoints of the loaded program
 * @param[in] lines        The tags of the edited text
 * @param[in] new_map      The source map of the edited text, from a new assembly
 */
[[nodiscard]]
auto remap_breakpoints(const std::set<uint16_t>& breakpoints, const LineTracker& lines, const SourceMap& new_map)
    -> RemappedBreakpoints;

} // namespace c8
