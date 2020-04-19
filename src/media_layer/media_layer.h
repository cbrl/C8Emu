#pragma once

#include <SDL2/SDL.h>

class chip8;

class MediaLayer {
public:
    MediaLayer();
    ~MediaLayer();

    void process_events(bool& quit);
    void render(const chip8& chip);

private:

    // The SDL window and its scale
    SDL_Window* window;
    uint8_t scale = 4;
};