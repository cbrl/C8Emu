#pragma once

#include <expected>
#include <filesystem>
#include <string>

#include "frontend/settings.h"

namespace c8 {

/// The name of the settings file in the user data folder
inline constexpr auto settings_file_name = "settings.json";

/**
 * @brief Read the settings from a JSON file
 *
 * @details A file that does not exist gives the default settings. A key that
 *          the settings do not have is ignored. A missing key keeps its
 *          default value. The values are clamped to their ranges.
 *
 * @return The settings, or an error message for a file that is not valid
 */
[[nodiscard]]
auto read_settings_file(const std::filesystem::path& file) -> std::expected<Settings, std::string>;

/**
 * @brief Write the settings to a JSON file
 *
 * @details The function writes a temporary file first, then replaces the
 *          file with it. If the write fails, the previous file stays.
 *
 * @return An error message on failure
 */
[[nodiscard]]
auto write_settings_file(const std::filesystem::path& file, const Settings& settings) -> std::expected<void, std::string>;

} // namespace c8
