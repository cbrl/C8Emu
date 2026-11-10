#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace c8 {

/**
 * @class Display
 *
 * @brief The CHIP-8 frame buffer
 *
 * @details Each pixel is a bitmask holding the on/off state for each plane.
 *          Plain CHIP-8 uses only plane bit 0.
 */
class Display {
public:
    static constexpr size_t max_width  = 128;
    static constexpr size_t max_height = 64;

    /// The number of bit planes. XO-CHIP draws to 2 planes, so a pixel has 4 colors.
    static constexpr size_t  plane_count = 2;
    static constexpr uint8_t all_planes  = (1u << plane_count) - 1;

    /// A 64x32 display
    Display() noexcept : Display(64, 32) {
    }

    explicit Display(size_t width, size_t height) noexcept {
        resize(width, height);
    }

    /// Change the size of the display and clear it. The size is clamped to [1, max].
    auto resize(size_t width, size_t height) noexcept -> void {
        w = std::clamp<size_t>(width, 1, max_width);
        h = std::clamp<size_t>(height, 1, max_height);
        clear();
    }

    [[nodiscard]]
    auto width() const noexcept -> size_t {
        return w;
    }

    [[nodiscard]]
    auto height() const noexcept -> size_t {
        return h;
    }

    /// The number of pixels (width * height)
    [[nodiscard]]
    auto size() const noexcept -> size_t {
        return w * h;
    }

    /// Clear the given plane bits of every pixel. The default clears all planes.
    auto clear(uint8_t planes = all_planes) noexcept -> void {
        if (planes == all_planes) {
            px.fill(0);
        }
        else {
            for (auto& value : std::span{px}.first(size())) {
                value = static_cast<uint8_t>(value & ~planes);
            }
        }
        ++change_count;
    }

    /// Move the given planes down by `rows`. The rows at the top become 0.
    auto scroll_down(size_t rows, uint8_t planes = all_planes) noexcept -> void {
        shift(0, static_cast<ptrdiff_t>(rows), planes);
    }

    /// Move the given planes up by `rows`. The rows at the bottom become 0.
    auto scroll_up(size_t rows, uint8_t planes = all_planes) noexcept -> void {
        shift(0, -static_cast<ptrdiff_t>(rows), planes);
    }

    /// Move the given planes left by `columns`. The columns at the right become 0.
    auto scroll_left(size_t columns, uint8_t planes = all_planes) noexcept -> void {
        shift(-static_cast<ptrdiff_t>(columns), 0, planes);
    }

    /// Move the given planes right by `columns`. The columns at the left become 0.
    auto scroll_right(size_t columns, uint8_t planes = all_planes) noexcept -> void {
        shift(static_cast<ptrdiff_t>(columns), 0, planes);
    }

    /// Get the plane bits of a pixel. A pixel outside the display is 0.
    [[nodiscard]]
    auto pixel(size_t x, size_t y) const noexcept -> uint8_t {
        return (x < w && y < h) ? px[(y * w) + x] : uint8_t{0};
    }

    /**
     * @brief XOR plane bits into a pixel
     *
     * @details A pixel outside the display does not change. The caller must
     *          clip or wrap the coordinates first.
     *
     * @return True if a set bit was cleared (a collision), otherwise false.
     */
    auto toggle(size_t x, size_t y, uint8_t planes = 1) noexcept -> bool {
        if (x >= w || y >= h) {
            return false;
        }

        auto& value = px[(y * w) + x];
        const bool collision = (value & planes) != 0;
        value = static_cast<uint8_t>(value ^ planes);
        ++change_count;
        return collision;
    }

    /// The pixels in row-major order (width * height values)
    [[nodiscard]]
    auto pixels() const noexcept -> std::span<const uint8_t> {
        return std::span{px}.first(size());
    }

    /**
     * @brief Set the size and all pixels, for example from a save state
     *
     * @details The size is clamped as in resize(). Each pixel keeps only its
     *          plane bits. If `values` has fewer pixels than the display, the
     *          other pixels are 0.
     */
    auto assign(size_t width, size_t height, std::span<const uint8_t> values) noexcept -> void {
        resize(width, height);

        const auto count = std::min(values.size(), size());
        for (size_t index = 0; index < count; ++index) {
            px[index] = static_cast<uint8_t>(values[index] & all_planes);
        }
    }

    /// A counter that increases on each change. A renderer can skip uploads when it does not change.
    [[nodiscard]]
    auto version() const noexcept -> uint64_t {
        return change_count;
    }

    /// Compare size and pixels. The version counter is not part of the comparison.
    friend auto operator==(const Display& lhs, const Display& rhs) noexcept -> bool {
        return lhs.w == rhs.w
            && lhs.h == rhs.h
            && std::ranges::equal(lhs.pixels(), rhs.pixels());
    }

private:
    // Move the plane bits by (dx, dy). Bits that move in from outside the display are 0.
    auto shift(ptrdiff_t dx, ptrdiff_t dy, uint8_t planes) noexcept -> void {
        const auto width  = static_cast<ptrdiff_t>(w);
        const auto height = static_cast<ptrdiff_t>(h);

        // Copy in the direction that reads each source pixel before it is overwritten
        const bool reverse_y = dy > 0;
        const bool reverse_x = dx > 0;

        for (ptrdiff_t row = 0; row < height; ++row) {
            const ptrdiff_t y = reverse_y ? (height - 1 - row) : row;
            const ptrdiff_t source_y = y - dy;

            for (ptrdiff_t column = 0; column < width; ++column) {
                const ptrdiff_t x = reverse_x ? (width - 1 - column) : column;
                const ptrdiff_t source_x = x - dx;

                uint8_t source = 0;
                if ((source_x >= 0) && (source_x < width) && (source_y >= 0) && (source_y < height)) {
                    source = px[static_cast<size_t>((source_y * width) + source_x)];
                }

                auto& value = px[static_cast<size_t>((y * width) + x)];
                value = static_cast<uint8_t>((value & ~planes) | (source & planes));
            }
        }

        ++change_count;
    }

    size_t w = 64;
    size_t h = 32;
    std::array<uint8_t, max_width * max_height> px;
    uint64_t change_count = 0;
};

} // namespace c8
