#include "display_texture.h"

#include <algorithm>

namespace c8 {

DisplayTexture::DisplayTexture() {
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);

    // The ImGui backend binds its own sampler, which overrides these filters. The display panel selects
    // the nearest sampler. These values only apply when no sampler is bound.
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
}


DisplayTexture::~DisplayTexture() {
    glDeleteTextures(1, &texture);
}


auto DisplayTexture::update(const Display& display, const Palette& colors) -> void {
    const auto pixels = display.pixels();

    const bool resized = (display.width() != width) || (display.height() != height);
    const bool recolored = colors != palette;

    // A display has at most 8192 pixels, so the compare costs less than an upload
    if (!resized && !recolored && std::ranges::equal(pixels, uploaded_pixels)) {
        return;
    }

    uploaded_pixels.assign(pixels.begin(), pixels.end());
    width   = display.width();
    height  = display.height();
    palette = colors;

    // The display holds pixel bits. The colors belong to the frontend.
    rgba.resize(pixels.size() * 4);
    for (size_t idx = 0; idx < pixels.size(); ++idx) {
        const uint32_t color = palette[static_cast<size_t>(pixels[idx] & Display::all_planes)];
        rgba[(idx * 4) + 0] = static_cast<uint8_t>(color >> 24);
        rgba[(idx * 4) + 1] = static_cast<uint8_t>(color >> 16);
        rgba[(idx * 4) + 2] = static_cast<uint8_t>(color >> 8);
        rgba[(idx * 4) + 3] = static_cast<uint8_t>(color);
    }

    glBindTexture(GL_TEXTURE_2D, texture);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    if (resized) {
        glTexImage2D(
            GL_TEXTURE_2D,
			0,
			GL_RGBA8,
            static_cast<GLsizei>(width),
			static_cast<GLsizei>(height),
            0,
			GL_RGBA,
			GL_UNSIGNED_BYTE,
			rgba.data()
        );
    }
    else {
        glTexSubImage2D(
            GL_TEXTURE_2D,
			0,
			0,
			0,
            static_cast<GLsizei>(width),
			static_cast<GLsizei>(height),
            GL_RGBA,
			GL_UNSIGNED_BYTE,
			rgba.data()
        );
    }
}

} // namespace c8
