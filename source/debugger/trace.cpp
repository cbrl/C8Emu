#include "trace.h"

#include <algorithm>

namespace c8 {

auto Trace::start() -> void {
    if (entries.empty()) {
        entries.resize(capacity);
    }
    recording = true;
}


auto Trace::record(const MachineState& state, uint16_t opcode) noexcept -> void {
    if (!recording || entries.empty()) {
        return;
    }

    entries[static_cast<size_t>(total % capacity)] = TraceEntry{
        .cycle  = state.total_cycles,
        .pc     = state.pc,
        .opcode = opcode,
        .i      = state.i,
        .sp     = state.sp,
        .v      = state.v,
    };
    ++total;
}


auto Trace::size() const noexcept -> size_t {
    return static_cast<size_t>(std::min<uint64_t>(total, capacity));
}

} // namespace c8
