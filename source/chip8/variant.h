#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>

namespace c8 {

/**
 * @brief The instruction set and the hardware of a CHIP-8 interpreter
 *
 * @details Each variant adds to the one before it, except Hires CHIP-8.
 */
enum class Variant : uint8_t {
    chip8,       ///< The COSMAC VIP interpreter: 64x32 display, 4 KiB memory
    chip8_hires, ///< The two-page VIP interpreter: 64x64 display. A program starts with 1260.
    schip,       ///< SUPER-CHIP 1.1: 128x64 hires mode, scrolling, a big font, and flag registers
    xochip,      ///< XO-CHIP: SUPER-CHIP plus 64 KiB memory, two bit planes, and sampled audio
};

inline constexpr auto all_variants = std::array{
    Variant::chip8,
    Variant::chip8_hires,
    Variant::schip,
    Variant::xochip,
};

/// The name of a variant for the user, for example "SUPER-CHIP"
[[nodiscard]]
constexpr auto to_string(Variant variant) noexcept -> std::string_view {
    switch (variant) {
        case Variant::chip8:       return "CHIP-8";
        case Variant::chip8_hires: return "Hires CHIP-8";
        case Variant::schip:       return "SUPER-CHIP";
        case Variant::xochip:      return "XO-CHIP";
    }
    return "Unknown";
}

/// The short name of a variant for files and the command line, for example "schip"
[[nodiscard]]
constexpr auto variant_id(Variant variant) noexcept -> std::string_view {
    switch (variant) {
        case Variant::chip8:       return "chip8";
        case Variant::chip8_hires: return "hires";
        case Variant::schip:       return "schip";
        case Variant::xochip:      return "xochip";
    }
    return "chip8";
}

/// Find a variant by its short name
[[nodiscard]]
constexpr auto variant_from_id(std::string_view id) noexcept -> std::optional<Variant> {
    for (const auto variant : all_variants) {
        if (variant_id(variant) == id) {
            return variant;
        }
    }
    return std::nullopt;
}

/// The size of the memory in bytes. It is a power of two.
[[nodiscard]]
constexpr auto memory_size(Variant variant) noexcept -> size_t {
    return (variant == Variant::xochip) ? 0x10000 : 0x1000;
}

/// Hires CHIP-8: a program starts with this jump to the interpreter extension at 0x260
inline constexpr uint16_t hires_entry_jump = 0x1260;

/// Hires CHIP-8: the first address of the program after the interpreter extension
inline constexpr uint16_t hires_program_start = 0x2C0;

/// True if the variant has the SUPER-CHIP instructions (hires mode, scrolling, flags)
[[nodiscard]]
constexpr auto has_schip(Variant variant) noexcept -> bool {
    return (variant == Variant::schip) || (variant == Variant::xochip);
}

/// True if the variant has the XO-CHIP instructions
[[nodiscard]]
constexpr auto has_xochip(Variant variant) noexcept -> bool {
    return variant == Variant::xochip;
}

} // namespace c8
