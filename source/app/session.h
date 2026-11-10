#pragma once

#include <cstdint>
#include <expected>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>

#include "chip8/variant.h"
#include "frontend/settings.h"

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
 *
 *          The session also follows the machine configuration of a ROM file.
 *          When the user changes the platform, the quirks, or the speed of a
 *          ROM, take_profile_change() gives the new configuration, and the app
 *          keeps it for the next open of the ROM.
 */
class Session {
public:
    /// The session keeps references to the machine and the debugger. Both must outlive it.
    Session(Chip8& machine, Debugger& run_control) noexcept;

    /**
     * @brief Load a ROM file and run it
     *
     * @details If the settings have a profile for the ROM, the machine uses
     *          it. If not, the extension of the file can identify the platform.
     *          If not and auto-detection is on, the code of the program is
     *          analyzed to identify it (see detect_platform()). A change of
     *          platform also selects the usual preset of the platform.
     *
     * @return False on failure. The machine does not change on failure.
     */
    auto open_rom(const std::filesystem::path& file, const Settings& settings) -> bool;

    /**
     * @brief Load an assembled program and run it
     *
     * @details A change of platform also selects the usual preset of the platform.
     *          The source name is for display.
     *
     * @return False on failure. The machine does not change on failure.
     */
    auto load_assembled(std::span<const uint8_t> program, Variant variant, std::string_view source_name) -> bool;

    /// Reset the machine and run the last program again
    auto reset() -> void;

    /**
     * @brief Note that a save state loaded
     *
     * @details The name of the program comes from the save state. If the
     *          program is not the one that loaded before, it is not a ROM file
     *          now, so it has no profile.
     */
    auto state_loaded(std::string_view state_name, bool same_program) -> void;

    /// The CRC-32 of the ROM file in the machine. std::nullopt if the program is not from a ROM file.
    [[nodiscard]]
    auto rom_crc() const noexcept -> std::optional<uint32_t> {
        return crc;
    }

    /// A change of the platform, the quirks, or the speed of the ROM since the last call. Gives each change once.
    [[nodiscard]]
    auto take_profile_change() -> std::optional<std::pair<uint32_t, RomProfile>>;

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
    // Load a program on a platform. A change of platform also selects the usual preset of the platform.
    [[nodiscard]]
    auto load(std::span<const uint8_t> program, Variant variant) -> std::expected<void, std::string>;

    // Note the ROM file that is in the machine now
    auto loaded_rom(const std::filesystem::path& file, uint32_t rom_crc) -> void;

    // The configuration of the machine now
    [[nodiscard]]
    auto current_profile() const noexcept -> RomProfile;

    Chip8& chip;
    Debugger& debugger;

    std::filesystem::path rom_file;
    std::string name;

    // The ROM file in the machine, and the configuration that take_profile_change() gave last
    std::optional<uint32_t> crc;
    RomProfile known_profile;
};

} // namespace c8
