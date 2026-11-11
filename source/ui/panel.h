#pragma once

#include <array>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <imgui.h>

#include "app/commands.h"

namespace c8 {

class Chip8;
class Debugger;
class Session;
class SourceDocument;
struct EmulationRate;
struct MachineState;
struct Settings;

/// A panel that can show an address
enum class ShowTarget : uint8_t {
    code_editor, ///< The source line that wrote the address. The Program panel shows an address that no line wrote.
    program,     ///< The instruction row of the address
    memory,      ///< The byte of the address
    sprites,     ///< The sprite at the address
    log,         ///< The Log panel. The address is not used.
};

/// A request to bring a panel to the front and show an address in it
struct ShowRequest {
    ShowTarget target  = ShowTarget::code_editor;
    uint32_t   address = 0;
};

/// Requests from the UI that the app handles after the frame
struct UiRequests {
    std::vector<Command> commands; ///< Oldest first

    /// The file of Command::open_rom_file, Command::open_source_file, and Command::open_state_file
    std::filesystem::path file;

    /// The user changed the memory, the registers, or the seed. The history starts again.
    bool state_edited = false;

    /// The CHIP-8 keys that the mouse holds on the Keypad panel (bit n is key n).
    /// A panel that is closed or hidden holds no keys.
    uint16_t keypad_keys = 0;

    /// Show an address in a panel. The UI does this itself at the end of the frame.
    std::optional<ShowRequest> show;

    auto push(Command command) -> void {
        commands.push_back(command);
    }

    /// Request to show an address in a panel. A later request in the same frame replaces it.
    auto show_in(ShowTarget target, uint32_t address = 0) -> void {
        show = ShowRequest{.target = target, .address = address};
    }

    /// Request a command that opens a file
    auto open(Command command, std::filesystem::path path) -> void {
        file = std::move(path);
        commands.push_back(command);
    }
};

/// What the panels can see and change
struct UiContext {
    Chip8&                             chip;
    Debugger&                          debugger;
    Session&                           session;
    SourceDocument&                    document;
    Settings&                          settings;
    const EmulationRate&               rate;
    const std::filesystem::path&       user_data;      ///< The folder of the settings, the save states, and the screenshots
    const std::array<std::string, 16>& key_names;      ///< The keyboard key of each CHIP-8 key, indexed by key 0x0-0xF
    ImTextureID                        display_texture;
    bool                               turbo_active;   ///< The turbo key is held
    bool                               rewinding;      ///< The rewind key is held, and the history plays backward
    bool                               text_input;     ///< A text field or the code editor has the keyboard, so the CHIP-8 keys do not reach the machine
    const std::optional<Command>&      unsaved_prompt; ///< A command that waits for an answer about unsaved source changes
    UiRequests&                        requests;

    /// The machine at the stop before the last stop, so a panel can mark the values that changed since then.
    /// nullptr while the program runs. The UI sets it at the start of the frame.
    const MachineState*                previous_stop = nullptr;
};


/// The text color of a value that changed since the stop before (see UiContext::previous_stop)
inline constexpr auto changed_value_color = ImVec4{1.0f, 0.55f, 0.25f, 1.0f};


/**
 * @class Panel
 *
 * @brief A dockable window of the UI
 *
 * @details The title is also the ImGui window ID.
 */
class Panel {
public:
    Panel() = default;
    virtual ~Panel() = default;

    Panel(const Panel&) = delete;
    Panel(Panel&&) = delete;
    auto operator=(const Panel&) -> Panel& = delete;
    auto operator=(Panel&&) -> Panel& = delete;

    [[nodiscard]]
    virtual auto title() const -> const char* = 0;

    /// Draw the window. The UI calls this only while the panel is open.
    virtual auto draw(UiContext& context) -> void = 0;

    /// Open the panel, and bring it to the front in the next frame. In a dock node, it becomes the selected tab.
    auto bring_to_front() noexcept -> void {
        open = true;
        focus_requested = true;
    }

    /// False after the user closes the window. The View menu opens it again.
    bool open = true;

protected:
    /// Call just before ImGui::Begin. It does the request of bring_to_front().
    auto apply_focus_request() -> void {
        if (focus_requested) {
            ImGui::SetNextWindowFocus();
            focus_requested = false;
        }
    }

private:
    bool focus_requested = false;
};

} // namespace c8
