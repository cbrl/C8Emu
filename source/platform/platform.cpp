#include "platform.h"

#include "util/log.h"

#include <algorithm>
#include <format>
#include <iterator>
#include <stdexcept>
#include <string_view>

#include <glad/glad.h>

#include <imgui.h>
#include <imgui_impl_opengl3.h>
#include <imgui_impl_sdl3.h>

namespace c8 {

namespace {

constexpr int default_window_width  = 1600;
constexpr int default_window_height = 1200;

// SDL writes its own messages here before the log hook replaces it
SDL_LogOutputFunction default_log_output = nullptr;


auto sdl_error(std::string_view what) -> std::runtime_error {
    return std::runtime_error{std::format("{}: {}", what, SDL_GetError())};
}


auto to_log_level(SDL_LogPriority priority) noexcept -> LogLevel {
    switch (priority) {
        case SDL_LOG_PRIORITY_WARN:     return LogLevel::warning;
        case SDL_LOG_PRIORITY_ERROR:    return LogLevel::error;
        case SDL_LOG_PRIORITY_CRITICAL: return LogLevel::error;
        case SDL_LOG_PRIORITY_INFO:     return LogLevel::info;
        default:                        return LogLevel::debug;
    }
}


// Send the SDL messages to the application log, and to the default SDL output
auto SDLCALL forward_sdl_log(void* user_data, int category, SDL_LogPriority priority, const char* message) -> void {
    app_log().write(to_log_level(priority), std::format("SDL: {}", (message != nullptr) ? message : ""));

    if (default_log_output != nullptr) {
        default_log_output(user_data, category, priority, message);
    }
}


// The UI scale for ImGui coordinates. ImGui handles the pixel density (for example, Retina) itself.
auto window_ui_scale(SDL_Window* window) -> float {
    const float display_scale = SDL_GetWindowDisplayScale(window);
    const float pixel_density = SDL_GetWindowPixelDensity(window);

    if ((display_scale <= 0.0f) || (pixel_density <= 0.0f)) {
        return 1.0f;
    }
    return display_scale / pixel_density;
}

} // namespace


auto default_keymap() noexcept -> Keymap {
    // CHIP-8 keypad    Keyboard
    //   1 2 3 C        1 2 3 4
    //   4 5 6 D        Q W E R
    //   7 8 9 E        A S D F
    //   A 0 B F        Z X C V
    auto keymap = Keymap{};

    keymap[0x1] = SDL_SCANCODE_1;
    keymap[0x2] = SDL_SCANCODE_2;
    keymap[0x3] = SDL_SCANCODE_3;
    keymap[0xC] = SDL_SCANCODE_4;

    keymap[0x4] = SDL_SCANCODE_Q;
    keymap[0x5] = SDL_SCANCODE_W;
    keymap[0x6] = SDL_SCANCODE_E;
    keymap[0xD] = SDL_SCANCODE_R;

    keymap[0x7] = SDL_SCANCODE_A;
    keymap[0x8] = SDL_SCANCODE_S;
    keymap[0x9] = SDL_SCANCODE_D;
    keymap[0xE] = SDL_SCANCODE_F;

    keymap[0xA] = SDL_SCANCODE_Z;
    keymap[0x0] = SDL_SCANCODE_X;
    keymap[0xB] = SDL_SCANCODE_C;
    keymap[0xF] = SDL_SCANCODE_V;

    return keymap;
}


//----------------------------------------------------------------------------------
// SdlLibrary
//----------------------------------------------------------------------------------

Platform::SdlLibrary::SdlLibrary() {
    // Set the log hook first, so the messages of SDL_Init reach the log
    default_log_output = SDL_GetDefaultLogOutputFunction();
    SDL_SetLogOutputFunction(forward_sdl_log, nullptr);

    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS)) {
        throw sdl_error("SDL_Init failed");
    }

    // The emulator runs without sound if no audio device exists
    if (!SDL_InitSubSystem(SDL_INIT_AUDIO)) {
        log_warning("Audio is not available: {}", SDL_GetError());
    }
}


Platform::SdlLibrary::~SdlLibrary() {
    SDL_Quit();
    SDL_SetLogOutputFunction(default_log_output, nullptr);
}


//----------------------------------------------------------------------------------
// ImGuiLayer
//----------------------------------------------------------------------------------

Platform::ImGuiLayer::ImGuiLayer(SDL_Window* window, SDL_GLContext context) {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    auto& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.ConfigDockingWithShift = false;

    if (!ImGui_ImplSDL3_InitForOpenGL(window, context)) {
        ImGui::DestroyContext();
        throw std::runtime_error{"Cannot start the ImGui SDL3 backend"};
    }

    if (!ImGui_ImplOpenGL3_Init("#version 330")) {
        ImGui_ImplSDL3_Shutdown();
        ImGui::DestroyContext();
        throw std::runtime_error{"Cannot start the ImGui OpenGL3 backend"};
    }
}


Platform::ImGuiLayer::~ImGuiLayer() {
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();
}


//----------------------------------------------------------------------------------
// Platform
//----------------------------------------------------------------------------------

Platform::Platform() :
    sdl_window(create_window()),
    gl_context(create_context(sdl_window.get())),
    imgui(sdl_window.get(), gl_context.get()) {

    apply_ui_scale();

    // The pixel font is clean only at its own size. A larger UI needs the scalable font.
    auto& io = ImGui::GetIO();
    if (ui_scale > 1.0f) {
        io.Fonts->AddFontDefaultVector();
    }
    else {
        io.Fonts->AddFontDefault();
    }

    log_info("OpenGL {}, {}", reinterpret_cast<const char*>(glGetString(GL_VERSION)), reinterpret_cast<const char*>(glGetString(GL_RENDERER)));
}


Platform::~Platform() = default;


auto Platform::create_window() -> WindowPtr {
    // Set the context attributes before the window exists. Some platforms choose the pixel format with the window.
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
#ifdef __APPLE__
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG);
#endif
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

    // Scale the window to the display, but keep it on the screen
    const auto display = SDL_GetPrimaryDisplay();
    float scale = SDL_GetDisplayContentScale(display);
    if (scale <= 0.0f) {
        scale = 1.0f;
    }

    int width  = static_cast<int>(static_cast<float>(default_window_width) * scale);
    int height = static_cast<int>(static_cast<float>(default_window_height) * scale);

    auto usable = SDL_Rect{};
    if (SDL_GetDisplayUsableBounds(display, &usable)) {
        width  = std::min(width, (usable.w * 9) / 10);
        height = std::min(height, (usable.h * 9) / 10);
    }

    auto window = WindowPtr{SDL_CreateWindow(
        "CHIP-8",
        width,
        height,
        SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY
    )};

    if (!window) {
        throw sdl_error("Cannot create the window");
    }
    return window;
}


auto Platform::create_context(SDL_Window* window) -> ContextPtr {
    auto context = ContextPtr{SDL_GL_CreateContext(window)};
    if (!context) {
        throw sdl_error("Cannot create an OpenGL 3.3 context");
    }

    if (!SDL_GL_MakeCurrent(window, context.get())) {
        throw sdl_error("Cannot activate the OpenGL context");
    }

    // VSync stays off. Then only the frame limit sets the pace of the main loop.
    (void)SDL_GL_SetSwapInterval(0);

    if (gladLoadGLLoader(reinterpret_cast<GLADloadproc>(SDL_GL_GetProcAddress)) == 0) {
        throw std::runtime_error{"Cannot load the OpenGL functions"};
    }

    return context;
}


auto Platform::apply_ui_scale() -> void {
    const float scale = window_ui_scale(sdl_window.get());
    if (scale == ui_scale) {
        return;
    }
    ui_scale = scale;

    // ScaleAllSizes() multiplies the current sizes, so start from a new style each time
    auto style = ImGuiStyle{};
    ImGui::StyleColorsDark(&style);
    style.ScaleAllSizes(scale);
    style.FontScaleDpi = scale;
    ImGui::GetStyle() = style;

    log_debug("UI scale: {:.2f}", scale);
}


auto Platform::poll_events(const Keymap& keymap) -> PlatformEvents {
    auto events = PlatformEvents{};
    auto event = SDL_Event{};

    while (SDL_PollEvent(&event)) {
        ImGui_ImplSDL3_ProcessEvent(&event);

        switch (event.type) {
            case SDL_EVENT_QUIT: {
                events.quit = true;
                break;
            }

            case SDL_EVENT_WINDOW_FOCUS_LOST: {
                // The window does not see the key releases while it has no focus
                events.release_all_keys = true;
                events.keys.clear();
                break;
            }

            case SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED: {
                apply_ui_scale();
                break;
            }

            case SDL_EVENT_KEY_DOWN:
            case SDL_EVENT_KEY_UP: {
                const bool down = (event.type == SDL_EVENT_KEY_DOWN);

                // ImGui uses the keyboard while a text field or the code editor has focus.
                // A key with Ctrl, Alt, or GUI is a shortcut (for example, Ctrl+R), not CHIP-8 input.
                constexpr auto shortcut_mods = static_cast<SDL_Keymod>(SDL_KMOD_CTRL | SDL_KMOD_ALT | SDL_KMOD_GUI);
                if (down && (event.key.repeat || ImGui::GetIO().WantCaptureKeyboard || ((event.key.mod & shortcut_mods) != 0))) {
                    break;
                }

                const auto it = std::ranges::find(keymap, event.key.scancode);
                if (it != keymap.end()) {
                    const auto key = static_cast<uint8_t>(std::distance(keymap.begin(), it));
                    events.keys.push_back(KeyEvent{.key = key, .down = down});
                }
                break;
            }

            default: {
                break;
            }
        }
    }

    return events;
}


auto Platform::begin_frame() -> void {
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();
}


auto Platform::end_frame() -> void {
    ImGui::Render();

    // The framebuffer size is in pixels. With a high pixel density, it is larger than the window size.
    int width  = 0;
    int height = 0;
    SDL_GetWindowSizeInPixels(sdl_window.get(), &width, &height);
    glViewport(0, 0, width, height);

    glClearColor(0.45f, 0.55f, 0.60f, 1.00f);
    glClear(GL_COLOR_BUFFER_BIT);

    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    SDL_GL_SwapWindow(sdl_window.get());
}


auto Platform::limit_frame_rate(bool enabled, uint32_t frames_per_second) -> void {
    if (!enabled || (frames_per_second == 0)) {
        return;
    }

    const Uint64 frame_ns = static_cast<Uint64>(SDL_NS_PER_SECOND) / frames_per_second;

    next_frame_ns += frame_ns;

    const Uint64 now = SDL_GetTicksNS();
    if (next_frame_ns > now) {
        SDL_DelayPrecise(next_frame_ns - now);
    }
    else if ((now - next_frame_ns) > frame_ns) {
        // More than one frame late. Do not try to catch up.
        next_frame_ns = now;
    }
}

} // namespace c8
