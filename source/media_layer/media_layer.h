#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

#include <SDL3/SDL.h>
#include <glad/glad.h>

#include <imgui.h>
#include <imgui_memory_editor.h>
#include <imgui-color-text-edit/TextEditor.h>

#include "gui_widgets/file_selector.h"
#include "beeper/beeper.h"


namespace c8 {
class Chip8;
}

class MediaLayer {
public:
    MediaLayer();
    ~MediaLayer();

    /**
     * @brief Processes pending SDL events
     *
     * @param[in] chip  A reference to an instance of a Chip8, for passing input state.
     * @param[in] quit  A reference to a flag that indicates when to quit the main loop.
     */
    auto process_events(c8::Chip8& chip, bool& quit) -> void;

    /**
     * @brief Render the GUI
     *
     * @param[in] chip  A reference to an instance of a Chip8 to render the GUI for
     */
    auto render(c8::Chip8& chip) -> void;

    /**
     * @brief Set the display scaling
     * @details The CHIP-8 display is only 64x32, so it needs to be scaled
     *          in order to be sufficiently visible on a modern display.
     *
     * @param[in] scale  The integer scaling for the display
     */
    auto set_display_scale(uint32_t scale) -> void;

private:

    auto begin_frame() -> void;
    auto end_frame() -> void;
    auto render_ui(c8::Chip8& chip) -> void;

    // Convert the CHIP-8 pixels to colors and upload them to the display texture
    auto update_display_texture(const c8::Chip8& chip) -> void;

    // Load a ROM file, then show its disassembly in the code editor
    auto load_rom_file(c8::Chip8& chip, const std::filesystem::path& file) -> void;

    // Dock the windows in the default layout
    auto build_default_layout(ImGuiID dockspace_id) -> void;

    // Show the breakpoints of the CHIP-8 as markers in the code editor
    auto update_breakpoint_markers(const c8::Chip8& chip) -> void;


    // The SDL window
    SDL_Window* window = nullptr;

    // The OpenGL context for the SDL window
    SDL_GLContext gl_context = {};

    // The CHIP-8 display texture, its scale, and its colors (RGBA)
    GLuint texture = 0;
    uint32_t display_scale = 10;
    uint32_t background_color = 0x000000FF;
    uint32_t foreground_color = 0xFFFFFFFF;
    std::vector<uint8_t> texture_pixels;

    // CHIP-8 audio output
    Beeper beeper;

    // When true, the next frame docks the windows in the default layout
    bool reset_layout = false;

    // GUI widgets
    FileSelector file_selector;
    MemoryEditor mem_editor;
    TextEditor text_editor;

    // Instruction Window state
    int instruction_count = 10;
    std::vector<std::string> instructions;

    // The mapping from keyboard keys to CHIP-8 keys (0x0-0xF)
    static inline const std::unordered_map<SDL_Scancode, uint8_t> key_map = {
        {SDL_SCANCODE_KP_0, 0x0},
        {SDL_SCANCODE_KP_1, 0x1},
        {SDL_SCANCODE_KP_2, 0x2},
        {SDL_SCANCODE_KP_3, 0x3},
        {SDL_SCANCODE_KP_4, 0x4},
        {SDL_SCANCODE_KP_5, 0x5},
        {SDL_SCANCODE_KP_6, 0x6},
        {SDL_SCANCODE_KP_7, 0x7},
        {SDL_SCANCODE_KP_8, 0x8},
        {SDL_SCANCODE_KP_9, 0x9},
        {SDL_SCANCODE_A,    0xA},
        {SDL_SCANCODE_B,    0xB},
        {SDL_SCANCODE_C,    0xC},
        {SDL_SCANCODE_D,    0xD},
        {SDL_SCANCODE_E,    0xE},
        {SDL_SCANCODE_F,    0xF},
    };
};
