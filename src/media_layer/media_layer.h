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

    /**
     * @brief Processes pending SDL events
     * 
     * @param[in] chip  A reference to an instance of a chip8, for passing input state.
     * @param[in] quit  A reference to a flag that indicates when to quit the main loop.
     */
    void process_events(chip8& chip, bool& quit);

    /**
     * @brief Render the GUI
     * 
     * @param[in] chip  A reference to an instance of a chip8 to render the GUI for
     */
    void render(chip8& chip);

    /**
     * @brief Set the display scaling
     * @details The CHIP-8 display is only 64x32, so it needs to be scaled
     *          in order to be sufficiently visible on a modern display.
     *
     * @param[in] scale  The integer scaling for the display
     */
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

    // GUI widgets
    FileSelector file_selector;
    MemoryEditor mem_editor;
};