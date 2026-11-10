#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include "chip8/machine_state.h"
#include "chip8/variant.h"

namespace c8 {

/**
 * @class Profiler
 *
 * @brief Counts the executions of each address and the data reads and writes of each byte
 *
 * @details The profiler allocates its counters when it starts, so a profiler
 *          that never runs costs no memory. The counters have one entry for
 *          each byte of memory.
 */
class Profiler {
public:
    /// Start to count. The counts of an earlier run stay if the memory size did not change.
    auto start(size_t memory_size) -> void;

    /// Stop to count. The counts stay.
    auto stop() noexcept -> void {
        running = false;
    }

    [[nodiscard]]
    auto is_running() const noexcept -> bool {
        return running;
    }

    /// Set all counts to 0, with one counter for each byte of a memory of this size
    auto clear(size_t memory_size) -> void;

    /// Count the data that the instruction at the PC will read and write. Call before the instruction runs.
    auto count_access(const MachineState& state, uint16_t opcode, Variant variant) noexcept -> void;

    /// Count an execution of the instruction at an address
    auto count_execution(uint32_t address) noexcept -> void {
        if (running && (address < executions.size())) {
            ++executions[address];
        }
    }

    /// The executions of the instruction at each address. Empty before the first start.
    [[nodiscard]]
    auto execution_counts() const noexcept -> std::span<const uint64_t> {
        return executions;
    }

    /// The data reads of each byte
    [[nodiscard]]
    auto read_counts() const noexcept -> std::span<const uint64_t> {
        return reads;
    }

    /// The data writes of each byte
    [[nodiscard]]
    auto write_counts() const noexcept -> std::span<const uint64_t> {
        return writes;
    }

private:
    std::vector<uint64_t> executions;
    std::vector<uint64_t> reads;
    std::vector<uint64_t> writes;
    bool running = false;
};

} // namespace c8
