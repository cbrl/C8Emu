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

    /// Set every pixel to 0
    auto clear() noexcept -> void {
        px.fill(0);
        ++change_count;
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
    size_t w = 64;
    size_t h = 32;
    std::array<uint8_t, max_width * max_height> px;
    uint64_t change_count = 0;
};

} // namespace c8
