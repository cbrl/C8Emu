#include "profiler.h"

#include "chip8/isa/isa.h"

namespace c8 {

auto Profiler::start(size_t memory_size) -> void {
    running = true;
    if (executions.size() != memory_size) {
        clear(memory_size);
    }
}


auto Profiler::clear(size_t memory_size) -> void {
    // Keep the memory of a profiler that never ran
    if (executions.empty() && !running) {
        return;
    }
    executions.assign(memory_size, 0);
    reads.assign(memory_size, 0);
    writes.assign(memory_size, 0);
}


auto Profiler::count_access(const MachineState& state, uint16_t opcode, Variant variant) noexcept -> void {
    if (!running || (reads.size() != state.memory.size())) {
        return;
    }

    const auto access = isa::memory_access(state, opcode, variant);
    const auto mask = state.mask();

    for (uint32_t offset = 0; offset < access.read_length; ++offset) {
        ++reads[(access.read_address + offset) & mask];
    }
    for (uint32_t offset = 0; offset < access.write_length; ++offset) {
        ++writes[(access.write_address + offset) & mask];
    }
}

} // namespace c8
