#pragma once

#include <expected>
#include <filesystem>
#include <optional>
#include <string>

#include <SDL3/SDL.h>

namespace c8 {

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
    FileDialogs();
    ~FileDialogs();

    FileDialogs(const FileDialogs&) = delete;
    FileDialogs(FileDialogs&&) = delete;
    auto operator=(const FileDialogs&) -> FileDialogs& = delete;
    auto operator=(FileDialogs&&) -> FileDialogs& = delete;

    /**
     * @brief Ask the user for a ROM file
     *
     * @param[in] parent  The window that owns the dialog
     * @param[in] folder  The folder that the dialog shows first. Empty for the system default.
     *
     * @return The chosen file, std::nullopt if the user cancelled, or an error message
     */
    [[nodiscard]]
    auto open_rom(SDL_Window* parent, const std::filesystem::path& folder)
        -> std::expected<std::optional<std::filesystem::path>, std::string>;

private:
    bool initialized = false;
    std::string init_error;
};

} // namespace c8
