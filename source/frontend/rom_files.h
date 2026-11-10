#pragma once

#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <optional>
#include <vector>

#include "chip8/variant.h"

namespace c8 {

/// Convert a path to UTF-8 text for messages. This never throws on Windows paths.
[[nodiscard]]
auto path_to_utf8(const std::filesystem::path& path) -> std::string;

/// Convert UTF-8 text (for example a path from SDL) to a path
[[nodiscard]]
auto path_from_utf8(std::string_view text) -> std::filesystem::path;

/**
 * @brief The platform that the extension of a ROM file names
 *
 * @details `.c8h` is Hires CHIP-8, `.sc8` is SUPER-CHIP, and `.xo8` is
 *          XO-CHIP. Other extensions, such as `.ch8`, name no platform.
 */
[[nodiscard]]
auto platform_from_extension(const std::filesystem::path& file) -> std::optional<Variant>;

/**
 * @brief Read a ROM file into memory
 *
 * @param[in] file      The path to the ROM file
 * @param[in] max_size  The largest accepted file size in bytes
 *
 * @return The file bytes, or an error message
 */
[[nodiscard]]
auto read_rom_file(const std::filesystem::path& file, size_t max_size) -> std::expected<std::vector<uint8_t>, std::string>;

/**
 * @brief Read a text file, such as an assembly source
 *
 * @details An empty file is valid. A UTF-8 byte order mark is removed.
 *
 * @return The text, or an error message
 */
[[nodiscard]]
auto read_text_file(const std::filesystem::path& file) -> std::expected<std::string, std::string>;

/// The extension of a save state file
inline constexpr std::string_view state_file_extension = ".c8state";

/// The folder of the save states in the user data folder
[[nodiscard]]
auto state_folder(const std::filesystem::path& user_data) -> std::filesystem::path;

/**
 * @brief The quick save file of a program and a slot
 *
 * @details The name is "<program>.<slot>.c8state", where <program> is the
 *          program name without its extension. A save state holds its
 *          program, so the name needs no checksum.
 */
[[nodiscard]]
auto state_slot_file(const std::filesystem::path& folder, std::string_view program_name, uint32_t slot) -> std::filesystem::path;

/// Write bytes to a file. An existing file is replaced. Returns an error message on failure.
[[nodiscard]]
auto write_file(const std::filesystem::path& file, std::span<const uint8_t> data) -> std::expected<void, std::string>;

/// Write text to a file without a change. An existing file is replaced. Returns an error message on failure.
[[nodiscard]]
auto write_text_file(const std::filesystem::path& file, std::string_view text) -> std::expected<void, std::string>;

} // namespace c8
