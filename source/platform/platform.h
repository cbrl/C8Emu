#pragma once

#include <array>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <SDL3/SDL.h>

namespace c8 {

/// A change of a CHIP-8 key
struct KeyEvent {
    uint8_t key  = 0;     ///< 0x0-0xF
    bool    down = false;
};

/// The input of one host frame
struct PlatformEvents {
    bool quit             = false;
    bool release_all_keys = false; ///< The window lost focus. Release all CHIP-8 keys before the key events.
    bool keymap_changed   = false; ///< The keyboard layout changed, so the names of the keys can be different
    std::vector<KeyEvent> keys;    ///< CHIP-8 key changes, oldest first

    /// The first file of a drop onto the window. A drop of more files gives only the first.
    std::optional<std::filesystem::path> dropped_file;
};

/// The keyboard key (scancode) for each CHIP-8 key, indexed by key 0x0-0xF
using Keymap = std::array<SDL_Scancode, 16>;

/// The COSMAC VIP keypad on the left side of the keyboard: 1234/QWER/ASDF/ZXCV
[[nodiscard]]
auto default_keymap() noexcept -> Keymap;

/// The name of the keyboard key of each CHIP-8 key in the current keyboard layout. SDL video must be ready.
[[nodiscard]]
auto key_names(const Keymap& keymap) -> std::array<std::string, 16>;


/**
 * @class Platform
 *
 * @brief The SDL window, its OpenGL 3.3 context, and Dear ImGui
 *
 * @details The constructor throws std::runtime_error if the window or the
 *          context cannot be created. Audio is optional: if the audio
 *          subsystem fails, the platform logs a warning and continues.
 *
 *          ImGui keeps the window layout (imgui.ini) in the user data folder.
 */
class Platform {
public:
    Platform();
    ~Platform();

    Platform(const Platform&) = delete;
    Platform(Platform&&) = delete;
    auto operator=(const Platform&) -> Platform& = delete;
    auto operator=(Platform&&) -> Platform& = delete;

    /**
     * @brief Read the pending events
     *
     * @details ImGui sees each event. A key press reaches the CHIP-8 only when
     *          ImGui does not use the keyboard (for example, when no text field
     *          has focus), and only without Ctrl, Alt, or GUI. A key release
     *          always reaches it. Key repeats do not.
     */
    [[nodiscard]]
    auto poll_events(const Keymap& keymap) -> PlatformEvents;

    /// Start a new ImGui frame
    auto begin_frame() -> void;

    /// Render the ImGui frame and show it
    auto end_frame() -> void;

    /// Sleep until the next frame. If `enabled` is false, return at once.
    auto limit_frame_rate(bool enabled, uint32_t frames_per_second) -> void;

    /// Set the text of the title bar of the window (UTF-8)
    auto set_title(const std::string& title) -> void;

    [[nodiscard]]
    auto window() const noexcept -> SDL_Window* {
        return sdl_window.get();
    }

    /**
     * @brief The folder for the settings, the save states, and the screenshots
     *
     * @details The folder is the SDL preference path of C8Emu (for example
     *          %APPDATA%\cbrl\C8Emu on Windows). If SDL cannot give it, the
     *          folder is the current folder.
     */
    [[nodiscard]]
    auto user_data_folder() const noexcept -> const std::filesystem::path& {
        return data_folder;
    }

private:
    // SDL_Init and SDL_Quit
    struct SdlLibrary {
        SdlLibrary();
        ~SdlLibrary();
        SdlLibrary(const SdlLibrary&) = delete;
        auto operator=(const SdlLibrary&) -> SdlLibrary& = delete;
    };

    // The ImGui context and its SDL3 and OpenGL3 backends
    struct ImGuiLayer {
        ImGuiLayer(SDL_Window* window, SDL_GLContext context);
        ~ImGuiLayer();
        ImGuiLayer(const ImGuiLayer&) = delete;
        auto operator=(const ImGuiLayer&) -> ImGuiLayer& = delete;
    };

    struct WindowDeleter {
        auto operator()(SDL_Window* window) const noexcept -> void {
            SDL_DestroyWindow(window);
        }
    };

    struct ContextDeleter {
        auto operator()(SDL_GLContext context) const noexcept -> void {
            SDL_GL_DestroyContext(context);
        }
    };

    using WindowPtr  = std::unique_ptr<SDL_Window, WindowDeleter>;
    using ContextPtr = std::unique_ptr<SDL_GLContextState, ContextDeleter>;

    [[nodiscard]]
    static auto create_window() -> WindowPtr;

    [[nodiscard]]
    static auto create_context(SDL_Window* window) -> ContextPtr;

    // Scale the ImGui style and fonts to the display scale of the window
    auto apply_ui_scale() -> void;

    // The members are destroyed in reverse order: ImGui, then the context, the window, and SDL.
    // ImGui writes the layout file when it stops, so the name of the file must outlive ImGui.
    SdlLibrary sdl;
    std::filesystem::path data_folder;
    std::string layout_file;
    WindowPtr  sdl_window;
    ContextPtr gl_context;
    ImGuiLayer imgui;

    float ui_scale = 0.0f;

    // A drop of files gave its first file. The later files of the drop are ignored.
    bool drop_file_taken = false;

    // The time of the next frame, for the frame limit (SDL_GetTicksNS)
    Uint64 next_frame_ns = 0;
};

} // namespace c8
