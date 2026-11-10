#pragma once

#include <array>
#include <cstdint>
#include <span>

namespace c8 {

namespace detail {

// The table of the reflected CRC-32 polynomial 0xEDB88320 (zlib, PNG)
inline constexpr auto crc32_table = [] {
    auto table = std::array<uint32_t, 256>{};
    for (uint32_t index = 0; index < table.size(); ++index) {
        uint32_t value = index;
        for (int bit = 0; bit < 8; ++bit) {
            value = ((value & 1u) != 0) ? (0xEDB88320u ^ (value >> 1)) : (value >> 1);
        }
        table[index] = value;
    }
    return table;
}();

} // namespace detail


/// The CRC-32 of bytes, as zlib computes it. It identifies a ROM file.
[[nodiscard]]
constexpr auto crc32(std::span<const uint8_t> bytes) noexcept -> uint32_t {
    uint32_t crc = 0xFFFFFFFFu;
    for (const auto byte : bytes) {
        crc = detail::crc32_table[(crc ^ byte) & 0xFFu] ^ (crc >> 8);
    }
    return crc ^ 0xFFFFFFFFu;
}

} // namespace c8
