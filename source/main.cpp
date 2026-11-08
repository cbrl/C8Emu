#include <exception>
#include <memory>

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include "app/app.h"

int main(int /*argc*/, char* /*argv*/[]) {
    try {
        const auto app = std::make_unique<c8::App>();
        app->run();
    }
    catch (const std::exception& e) {
        // A message box works without SDL_Init, so it also reports a failed start
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "C8Emu", e.what(), nullptr);
        return 1;
    }
    catch (...) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "C8Emu", "An unknown error occurred.", nullptr);
        return 1;
    }

    return 0;
}
