#include "source_map.h"

#include <algorithm>
#include <iterator>

namespace c8 {

auto SourceMap::add(const SourceSpan& span) -> void {
    if (span.size != 0) {
        span_list.push_back(span);
    }
}


auto SourceMap::span_containing(uint16_t address) const noexcept -> const SourceSpan* {
    // The first span that starts after the address. The span before it can hold the address.
    const auto after = std::ranges::upper_bound(span_list, address, {}, &SourceSpan::address);
    if (after == span_list.begin()) {
        return nullptr;
    }

    const auto& span = *std::prev(after);
    const uint32_t end = static_cast<uint32_t>(span.address) + span.size;
    return (address < end) ? &span : nullptr;
}


auto SourceMap::span_for_line(uint32_t line, bool snap_forward) const noexcept -> const SourceSpan* {
    const auto it = std::ranges::lower_bound(span_list, line, {}, &SourceSpan::line);
    if (it == span_list.end()) {
        return nullptr;
    }
    if (!snap_forward && (it->line != line)) {
        return nullptr;
    }
    return &*it;
}

} // namespace c8
