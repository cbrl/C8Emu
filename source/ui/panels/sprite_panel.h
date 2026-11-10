#pragma once

#include <cstdint>
#include <optional>
#include <string>

#include "ui/panel.h"

namespace c8 {

/**
 * @class SpritePanel
 *
 * @brief A viewer and editor for the sprite data in memory
 *
 * @details The panel shows the bytes at I, or at an address that the user
 *          gives, as a sprite of 8 pixels per row. It can also show a 16x16
 *          sprite (DXY0, 2 bytes per row). A click or a drag paints pixels
 *          into memory. "Copy as db" copies the rows as assembler source.
 */
class SpritePanel final : public Panel {
public:
    [[nodiscard]]
    auto title() const -> const char* override {
        return "Sprites";
    }

    auto draw(UiContext& context) -> void override;

private:
    // The address field. Enter sets the address from a debugger expression.
    auto draw_address_field(UiContext& context) -> void;

    // The grid of pixels. A click or a drag paints.
    auto draw_grid(UiContext& context, int row_count, uint32_t bytes_per_row) -> void;

    bool follow_i = true;
    uint32_t address = 0x200;
    std::string address_text;
    std::string address_error;
    int rows = 5;
    bool wide = false;

    // The value that a drag paints. The first pixel of the drag sets it.
    std::optional<bool> paint_value;
};

} // namespace c8
