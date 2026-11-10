#include "breakpoint_remap.h"

#include "assembler/source_map.h"
#include "debugger/line_tracker.h"

namespace c8 {

auto remap_breakpoints(const std::set<uint16_t>& breakpoints, const LineTracker& lines, const SourceMap& new_map)
    -> RemappedBreakpoints
{
    auto result = RemappedBreakpoints{};

    for (const auto address : breakpoints) {
        const auto* old_span = lines.loaded_map().span_containing(address);

        // Not in the loaded program, so no line holds it
        if (old_span == nullptr) {
            result.moves.emplace(address, address);
            continue;
        }

        const auto line = lines.line_of(address);
        const auto* new_span = line ? new_map.span_for_line(*line, false) : nullptr;

        if (new_span == nullptr) {
            result.dropped.push_back(address);
            continue;
        }

        const auto offset = static_cast<uint16_t>(address - old_span->address);
        const auto new_offset = (offset < new_span->size) ? offset : uint16_t{0};
        result.moves.emplace(address, static_cast<uint16_t>(new_span->address + new_offset));
    }

    return result;
}

} // namespace c8
