#pragma once

#include <cstdint>
#include <optional>
#include <span>

#include "chip8/isa/isa.h"

namespace c8 {

/// The memory accesses that a watchpoint stops on, as bits
enum class WatchAccess : uint8_t {
    read       = 1,
    write      = 2,
    read_write = 3,
};

/// A breakpoint on data memory. It stops before an instruction that accesses the memory.
struct Watchpoint {
    uint32_t    id        = 0;
    uint16_t    address   = 0;
    uint16_t    length    = 1;
    WatchAccess access    = WatchAccess::write;
    bool        enabled   = true;
    uint32_t    hit_count = 0;
};

/// The first watched byte that an instruction accesses
struct WatchHit {
    size_t   index   = 0;     ///< The index of the watchpoint in the list
    uint32_t address = 0;     ///< The watched address
    bool     write   = false; ///< The access is a write
};

/**
 * @brief Find the first enabled watchpoint that an access touches
 *
 * @details The watchpoints are checked in order. For each watchpoint, a write
 *          is checked before a read.
 *
 * @param[in] watchpoints  The watchpoints, in order
 * @param[in] access       The data access of the next instruction
 * @param[in] mask         The address mask of the memory
 */
[[nodiscard]]
auto find_watch_hit(std::span<const Watchpoint> watchpoints, const isa::MemoryAccess& access, uint32_t mask) noexcept
    -> std::optional<WatchHit>;

} // namespace c8
