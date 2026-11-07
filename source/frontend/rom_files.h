#pragma once

#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <string>
#include <vector>

namespace c8 {

/// Convert a path to UTF-8 text for messages. This never throws on Windows paths.
[[nodiscard]]
auto path_to_utf8(const std::filesystem::path& path) -> std::string;

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

} // namespace c8
