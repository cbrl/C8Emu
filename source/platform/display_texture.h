#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include <glad/glad.h>

#include "display/display.h"

namespace c8 {

/**
 * @class DisplayTexture
 *
 * @brief The CHIP-8 display as an OpenGL texture
 *
 * @details The texture uploads only when the pixels, the size, or the colors
 *          change. An OpenGL context must be current for the whole lifetime.
 */
class DisplayTexture {
public:
    DisplayTexture();
    ~DisplayTexture();

    DisplayTexture(const DisplayTexture&) = delete;
    DisplayTexture(DisplayTexture&&) = delete;
    auto operator=(const DisplayTexture&) -> DisplayTexture& = delete;
    auto operator=(DisplayTexture&&) -> DisplayTexture& = delete;

    /// Convert the pixels to RGBA colors and upload them if anything changed
    auto update(const Display& display, uint32_t foreground_rgba, uint32_t background_rgba) -> void;

    [[nodiscard]]
    auto id() const noexcept -> GLuint {
        return texture;
    }

private:
    GLuint texture = 0;

    // The state of the last upload
    std::vector<uint8_t> uploaded_pixels;
    size_t   width      = 0;
    size_t   height     = 0;
    uint32_t foreground = 0;
    uint32_t background = 0;

    std::vector<uint8_t> rgba;
};

} // namespace c8
