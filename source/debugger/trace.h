#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "chip8/machine_state.h"

namespace c8 {

/// One executed instruction and the registers before it
struct TraceEntry {
    uint64_t                cycle  = 0; ///< MachineState::total_cycles before the instruction
    uint16_t                pc     = 0;
    uint16_t                opcode = 0;
    uint16_t                i      = 0;
    uint8_t                 sp     = 0;
    std::array<uint8_t, 16> v{};
};


/**
 * @class Trace
 *
 * @brief A ring buffer of the newest executed instructions
 *
 * @details The buffer allocates its entries when the trace starts, so a trace
 *          that never runs costs no memory. Each entry has a sequence number:
 *          the count of entries before it since the last clear. A viewer can
 *          read only the entries that are new since its last look.
 */
class Trace {
public:
    static constexpr size_t capacity = 100'000;

    /// Start to record. The entries of an earlier recording stay.
    auto start() -> void;

    /// Stop to record. The entries stay.
    auto stop() noexcept -> void {
        recording = false;
    }

    [[nodiscard]]
    auto is_recording() const noexcept -> bool {
        return recording;
    }

    /// Record an instruction before it runs
    auto record(const MachineState& state, uint16_t opcode) noexcept -> void;

    /// Remove all entries. The sequence numbers start at 0 again.
    auto clear() noexcept -> void {
        total = 0;
    }

    /// The number of entries in the buffer
    [[nodiscard]]
    auto size() const noexcept -> size_t;

    /// The sequence number of the oldest entry in the buffer
    [[nodiscard]]
    auto first_sequence() const noexcept -> uint64_t {
        return total - size();
    }

    /// The sequence number of the next entry: the count of entries since the last clear
    [[nodiscard]]
    auto end_sequence() const noexcept -> uint64_t {
        return total;
    }

    /// The entry with a sequence number from first_sequence() to end_sequence() - 1
    [[nodiscard]]
    auto at(uint64_t sequence) const noexcept -> const TraceEntry& {
        return entries[static_cast<size_t>(sequence % capacity)];
    }

private:
    std::vector<TraceEntry> entries;
    uint64_t total = 0;
    bool recording = false;
};

} // namespace c8
