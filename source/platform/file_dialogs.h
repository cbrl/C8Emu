#pragma once

#include <cstdint>
#include <expected>
#include <filesystem>
#include <optional>
#include <string>

#include <SDL3/SDL.h>

namespace c8 {

/// The kinds of files that the dialogs show
enum class FileKind : uint8_t {
    rom,    ///< A CHIP-8 program (.ch8)
    source, ///< An assembly source (.c8s)
    state,  ///< A save state (.c8state)
};


/**
 * @class FileDialogs
 *
 * @brief Native file dialogs, from nativefiledialog-extended
 *
 * @details A dialog blocks the calling thread until the user closes it.
 *          Construct the object and show dialogs on the main thread only,
 *          after SDL starts its video subsystem.
 */
class FileDialogs {
public:
    /// The dialog result: the chosen file (std::nullopt if the user cancelled) or an error message
    using Result = std::expected<std::optional<std::filesystem::path>, std::string>;

    FileDialogs();
    ~FileDialogs();

    FileDialogs(const FileDialogs&) = delete;
    FileDialogs(FileDialogs&&) = delete;
    auto operator=(const FileDialogs&) -> FileDialogs& = delete;
    auto operator=(FileDialogs&&) -> FileDialogs& = delete;

    /**
     * @brief Ask the user for a file to open
     *
     * @param[in] parent  The window that owns the dialog
     * @param[in] kind    The kind of file
     * @param[in] folder  The folder that the dialog shows first. Empty for the system default.
     */
    [[nodiscard]]
    auto open_file(SDL_Window* parent, FileKind kind, const std::filesystem::path& folder) -> Result;

    /**
     * @brief Ask the user for a file to save
     *
     * @param[in] parent  The window that owns the dialog
     * @param[in] kind    The kind of file
     * @param[in] folder  The folder that the dialog shows first. Empty for the system default.
     * @param[in] name    The file name that the dialog suggests
     */
    [[nodiscard]]
    auto save_file(SDL_Window* parent, FileKind kind, const std::filesystem::path& folder, const std::string& name) -> Result;

private:
    bool initialized = false;
    std::string init_error;
};

} // namespace c8
