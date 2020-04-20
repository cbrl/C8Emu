#pragma once

#include <SDL2/SDL.h>
#include "gl3w/GL/gl3w.h"

#include "imgui/imgui.h"
#include "gui_widgets/imgui_memory_editor.h"
#include "gui_widgets/file_selector.h"

class chip8;

class MediaLayer {
public:
    MediaLayer();
    ~MediaLayer();

    void process_events(chip8& chip, bool& quit);
    void render(chip8& chip);

    void set_display_scale(uint8_t scale);

private:

    void begin_frame();
    void end_frame();
    void render_ui(chip8& chip);


    // The SDL window
    SDL_Window* window;

    // The OpenGL context for the SDL window
    SDL_GLContext gl_context;

    // The CHIP-8 display texture and its scale
    GLuint texture;
    uint32_t display_scale = 10;

    FileSelector file_selector;
    MemoryEditor mem_editor;
};