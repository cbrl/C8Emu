#include "media_layer.h"
#include "chip8/chip8.h"

#include "imgui/imgui.h"
#include "imgui/imgui_impl_opengl3.h"
#include "imgui/imgui_impl_sdl.h"

#include <iostream>


static const std::map<SDL_Scancode, Keys> key_map = {
    {SDL_SCANCODE_KP_0, Keys::Key0},
    {SDL_SCANCODE_KP_1, Keys::Key1},
    {SDL_SCANCODE_KP_2, Keys::Key2},
    {SDL_SCANCODE_KP_3, Keys::Key3},
    {SDL_SCANCODE_KP_4, Keys::Key4},
    {SDL_SCANCODE_KP_5, Keys::Key5},
    {SDL_SCANCODE_KP_6, Keys::Key6},
    {SDL_SCANCODE_KP_7, Keys::Key7},
    {SDL_SCANCODE_KP_8, Keys::Key8},
    {SDL_SCANCODE_KP_9, Keys::Key9},
    {SDL_SCANCODE_A,    Keys::KeyA},
    {SDL_SCANCODE_B,    Keys::KeyB},
    {SDL_SCANCODE_C,    Keys::KeyC},
    {SDL_SCANCODE_D,    Keys::KeyD},
    {SDL_SCANCODE_E,    Keys::KeyE},
    {SDL_SCANCODE_F,    Keys::KeyF},
};


MediaLayer::MediaLayer() {
    //--------------------------------------------------------------------------------
    // SDL Init
    //--------------------------------------------------------------------------------
    const int result = SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_EVENTS);

    if (result != 0) {
        std::cout << "SDL_Init Error: " << SDL_GetError() << '\n';
    }

    window = SDL_CreateWindow(
        "CHIP-8",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        1280,
        960,
        SDL_WINDOW_SHOWN | SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI
    );

    if (!window) {
        std::cout << "SDL_CreateWindow Error: " << SDL_GetError() << '\n';
    }


    //--------------------------------------------------------------------------------
    // OpenGL Init
    //--------------------------------------------------------------------------------
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);

    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);

    SDL_GL_SetSwapInterval(1);

    gl_context = SDL_GL_CreateContext(window);
    if (!gl_context) {
        std::cout << "SDL_GL_CreateContext Error: " << SDL_GetError() << '\n';
    }

    SDL_GL_MakeCurrent(window, gl_context);

    if (gl3wInit() != 0) {
        std::cout << "gl3w initialization failed\n";
    }

    // Create the CHIP-8 display texture
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);


    //--------------------------------------------------------------------------------
    // ImGui Init
    //--------------------------------------------------------------------------------
    ImGui::CreateContext();
    ImGui_ImplSDL2_InitForOpenGL(window, gl_context);
    ImGui_ImplOpenGL3_Init("#version 330");
}


MediaLayer::~MediaLayer() {
    if (window) {
        SDL_DestroyWindow(window);
    }

    SDL_Quit();
}


void MediaLayer::set_display_scale(uint8_t scale) {
    display_scale = scale;
}


void MediaLayer::process_events(chip8& chip, bool& quit) {
    SDL_Event event;

    while (SDL_PollEvent(&event)) {
        ImGui_ImplSDL2_ProcessEvent(&event);

        switch (event.type) {
            case SDL_QUIT: {
                quit = true;
            }
            break;

            case SDL_KEYDOWN: {
                const auto it = key_map.find(event.key.keysym.scancode);
                if (it != key_map.end()) {
                    chip.input.set_key_state(it->second, true);
                }
            }
            break;

            case SDL_KEYUP: {
                const auto it = key_map.find(event.key.keysym.scancode);
                if (it != key_map.end()) {
                    chip.input.set_key_state(it->second, false);
                }
            }
            break;

            default: break;            
        }
    }
}


void MediaLayer::render(chip8& chip) {
    begin_frame();
    render_ui(chip);
    end_frame();
}


void MediaLayer::begin_frame() {
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL2_NewFrame(window);
    ImGui::NewFrame();
}


void MediaLayer::end_frame() {
    ImGui::Render();

    const ImGuiIO& io = ImGui::GetIO();
    glViewport(0, 0, (int)io.DisplaySize.x, (int)io.DisplaySize.y);

    glClearColor(0.45f, 0.55f, 0.60f, 1.00f);
    glClear(GL_COLOR_BUFFER_BIT);

    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    SDL_GL_SwapWindow(window);
}


void MediaLayer::render_ui(chip8& chip) {
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexImage2D(
        GL_TEXTURE_2D,
        0,
        GL_RGBA,
        64, 32,
        false,
        GL_RGBA,
        GL_UNSIGNED_INT_8_8_8_8,
        chip.display.data()
    );

    ImGui::ShowDemoWindow();

    if (ImGui::Begin("CHIP-8", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        const float x_size = 64 * display_scale;
        const float y_size = 32 * display_scale;

        ImGui::BeginChild("Image", {x_size + 16, y_size + 16}, true);
        ImGui::Image((ImTextureID)(intptr_t)texture, {x_size, y_size});
        ImGui::EndChild();
    }
    ImGui::End();

    if (ImGui::Begin("Display Scale", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::InputInt("Scale", (int*)&display_scale);
    }
    ImGui::End();
}