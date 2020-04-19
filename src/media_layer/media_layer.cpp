#include "media_layer.h"
#include "chip8/chip8.h"

#include <iostream>


MediaLayer::MediaLayer() {
    const int result = SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_EVENTS);

    if (result != 0) {
        std::cout << "SDL_Init Error: " << SDL_GetError() << '\n';
    }

    window = SDL_CreateWindow(
        "CHIP-8",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        64 * scale,
        32 * scale,
        SDL_WINDOW_SHOWN
    );

    if (!window) {
        std::cout << "SDL_CreateWindow Error: " << SDL_GetError() << '\n';
    }
}


MediaLayer::~MediaLayer() {
    if (window) {
        SDL_DestroyWindow(window);
    }

    SDL_Quit();
}


void MediaLayer::process_events(bool& quit) {
    SDL_Event event;

    while (SDL_PollEvent(&event)) {
        switch (event.type) {
            case SDL_QUIT:
                quit = true;
                break;

            case SDL_KEYDOWN:
                break;

            default: break;            
        }
    }
}


void MediaLayer::render(const chip8& chip) {

}