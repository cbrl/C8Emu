#pragma once

#include <array>
#include <cstdint>

#include "ui/panel.h"

namespace c8 {

/// The CHIP-8 keys in the layout of the COSMAC VIP keypad, row by row
inline constexpr auto keypad_layout = std::array<uint8_t, 16>{
    0x1, 0x2, 0x3, 0xC,
    0x4, 0x5, 0x6, 0xD,
    0x7, 0x8, 0x9, 0xE,
    0xA, 0x0, 0xB, 0xF,
};


/**
 * @class KeypadPanel
 *
 * @brief An on-screen CHIP-8 keypad
 *
 * @details A key is held while the mouse button is down on it. The app adds
 *          these keys to the keys that the keyboard holds. The keypad shows
 *          all held keys, and the key that FX0A waits for.
 *
 *          The panel reports its keys in each frame that it draws. When the
 *          mouse button goes up, or the panel closes or is hidden, it reports
 *          no keys, so no key stays down.
 */
class KeypadPanel final : public Panel {
public:
    [[nodiscard]]
    auto title() const -> const char* override {
        return "Keypad";
    }

    auto draw(UiContext& context) -> void override;
};

} // namespace c8
