#pragma once

#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>

namespace c8 {

class Chip8;
class Debugger;

/**
 * @class Session
 *
 * @brief The loaded program and its source location
 *
 * @details The session loads ROM files and assembled programs, and resets
 *          the machine. It writes each result to the application log.
 */
class Session {
public:
    /// The session keeps references to the machine and the debugger. Both must outlive it.
    Session(Chip8& machine, Debugger& run_control) noexcept;

    /// Load a ROM file and run it. Returns false on failure. The machine does not change on failure.
    auto open_rom(const std::filesystem::path& file) -> bool;

    /// Load an assembled program and run it. The source name is for display. Returns false on failure.
    auto load_assembled(std::span<const uint8_t> program, std::string_view source_name) -> bool;

    /// Reset the machine and run the last program again
    auto reset() -> void;

    /// The last ROM file that loaded, or an empty path
    [[nodiscard]]
    auto rom_path() const noexcept -> const std::filesystem::path& {
        return rom_file;
    }

    /// The name of the loaded program, for display. Empty if no program loaded.
    [[nodiscard]]
    auto program_name() const noexcept -> const std::string& {
        return name;
    }

    /// The folder to show in the next Open dialog
    [[nodiscard]]
    auto browse_folder() const -> std::filesystem::path;

private:
    Chip8& chip;
    Debugger& debugger;

    std::filesystem::path rom_file;
    std::string name;
};

} // namespace c8
