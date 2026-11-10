#pragma once

#include <array>
#include <cstdint>
#include <string_view>

#include "chip8/variant.h"

namespace c8 {

/// How FX55 and FX65 change the I register.
enum class LoadStoreQuirk : uint8_t {
    increment_x_plus_1, ///< I = I + X + 1 (COSMAC VIP)
    increment_x,        ///< I = I + X (CHIP-48)
    unchanged,          ///< I does not change (SUPER-CHIP)
};


/**
 * @brief Behaviors that differ between CHIP-8 interpreters
 *
 * @details The defaults are the COSMAC VIP behaviors.
 */
struct Quirks {
    /// 8XY6/8XYE: true shifts VY into VX (VIP). False shifts VX in place.
    bool shift_uses_vy = true;

    /// FX55/FX65: the change to the I register.
    LoadStoreQuirk load_store = LoadStoreQuirk::increment_x_plus_1;

    /// BNNN: true jumps to XNN + VX (CHIP-48). False jumps to NNN + V0 (VIP).
    bool jump_uses_vx = false;

    /// 8XY1/8XY2/8XY3: true sets VF to 0 after the operation (VIP).
    bool vf_reset = true;

    /// DXYN: true wraps pixels past the screen edge. False clips them.
    bool wrap_sprites = false;

    /// DXYN: true waits for the next 60 Hz frame after a draw (VIP).
    bool display_wait = true;

    /// FX0A: true completes on key release (VIP). False completes on key press.
    bool key_wait_release = true;

    friend constexpr auto operator==(const Quirks&, const Quirks&) -> bool = default;
};


/// A named configuration of a known interpreter
struct Preset {
    std::string_view id;
    std::string_view name;
    Variant  variant;
    Quirks   quirks;
    uint32_t instructions_per_frame;
};


namespace detail {

inline constexpr auto vip_quirks = Quirks{
    .shift_uses_vy    = true,
    .load_store       = LoadStoreQuirk::increment_x_plus_1,
    .jump_uses_vx     = false,
    .vf_reset         = true,
    .wrap_sprites     = false,
    .display_wait     = true,
    .key_wait_release = true,
};

} // namespace detail


inline constexpr auto presets = std::array{
    Preset{
        .id      = "vip",
        .name    = "COSMAC VIP",
        .variant = Variant::chip8,
        .quirks  = detail::vip_quirks,
        .instructions_per_frame = 15,
    },
    Preset{
        .id      = "hires",
        .name    = "Hires COSMAC VIP",
        .variant = Variant::chip8_hires,
        .quirks  = detail::vip_quirks,
        .instructions_per_frame = 15,
    },
    Preset{
        .id      = "chip48",
        .name    = "CHIP-48",
        .variant = Variant::chip8,
        .quirks  = Quirks{
            .shift_uses_vy    = false,
            .load_store       = LoadStoreQuirk::increment_x,
            .jump_uses_vx     = true,
            .vf_reset         = false,
            .wrap_sprites     = false,
            .display_wait     = false,
            .key_wait_release = false,
        },
        .instructions_per_frame = 30,
    },
    Preset{
        .id      = "schip",
        .name    = "SUPER-CHIP",
        .variant = Variant::schip,
        .quirks  = Quirks{
            .shift_uses_vy    = false,
            .load_store       = LoadStoreQuirk::unchanged,
            .jump_uses_vx     = true,
            .vf_reset         = false,
            .wrap_sprites     = false,
            .display_wait     = false,
            .key_wait_release = false,
        },
        .instructions_per_frame = 30,
    },
    Preset{
        .id      = "xochip",
        .name    = "XO-CHIP",
        .variant = Variant::xochip,
        .quirks  = Quirks{
            .shift_uses_vy    = true,
            .load_store       = LoadStoreQuirk::increment_x_plus_1,
            .jump_uses_vx     = false,
            .vf_reset         = false,
            .wrap_sprites     = true,
            .display_wait     = false,
            .key_wait_release = false,
        },
        .instructions_per_frame = 1000,
    },
};


/// Find the preset that has exactly this variant and these quirks. Returns nullptr for a custom set.
[[nodiscard]]
auto find_preset(Variant variant, const Quirks& quirks) noexcept -> const Preset*;

/// Find a preset by its id (for example "vip"). Returns nullptr if no preset has the id.
[[nodiscard]]
auto find_preset(std::string_view id) noexcept -> const Preset*;

/// The usual preset for a variant
[[nodiscard]]
auto default_preset(Variant variant) noexcept -> const Preset&;

} // namespace c8
