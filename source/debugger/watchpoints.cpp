#include "watchpoints.h"

namespace c8 {

namespace {

auto has(WatchAccess access, WatchAccess bit) noexcept -> bool {
    return (static_cast<uint8_t>(access) & static_cast<uint8_t>(bit)) != 0;
}

// The first byte of an access in the range of a watchpoint
auto first_hit(const Watchpoint& watch, uint32_t begin, uint32_t length, uint32_t mask) noexcept -> std::optional<uint32_t> {
    const uint32_t watch_end = uint32_t{watch.address} + watch.length;

    for (uint32_t offset = 0; offset < length; ++offset) {
        const uint32_t address = (begin + offset) & mask;
        if ((address >= watch.address) && (address < watch_end)) {
            return address;
        }
    }
    return std::nullopt;
}

} // namespace


auto find_watch_hit(std::span<const Watchpoint> watchpoints, const isa::MemoryAccess& access, uint32_t mask) noexcept
    -> std::optional<WatchHit>
{
    if (access.empty()) {
        return std::nullopt;
    }

    for (size_t index = 0; index < watchpoints.size(); ++index) {
        const auto& watch = watchpoints[index];
        if (!watch.enabled) {
            continue;
        }

        if (has(watch.access, WatchAccess::write)) {
            if (const auto address = first_hit(watch, access.write_address, access.write_length, mask)) {
                return WatchHit{.index = index, .address = *address, .write = true};
            }
        }
        if (has(watch.access, WatchAccess::read)) {
            if (const auto address = first_hit(watch, access.read_address, access.read_length, mask)) {
                return WatchHit{.index = index, .address = *address, .write = false};
            }
        }
    }

    return std::nullopt;
}

} // namespace c8
