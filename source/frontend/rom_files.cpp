#include "rom_files.h"

#include <format>
#include <fstream>
#include <system_error>

namespace c8 {

auto path_to_utf8(const std::filesystem::path& path) -> std::string {
    const auto text = path.u8string();
    return std::string{reinterpret_cast<const char*>(text.data()), text.size()};
}


auto path_from_utf8(std::string_view text) -> std::filesystem::path {
    const auto utf8 = std::u8string{reinterpret_cast<const char8_t*>(text.data()), text.size()};
    return std::filesystem::path{utf8};
}


auto read_rom_file(const std::filesystem::path& file, size_t max_size) -> std::expected<std::vector<uint8_t>, std::string> {
    const auto name = path_to_utf8(file);
    auto ec = std::error_code{};

    const auto status = std::filesystem::status(file, ec);
    if (ec || !std::filesystem::exists(status)) {
        return std::unexpected(std::format("The file \"{}\" does not exist", name));
    }
    if (std::filesystem::is_directory(status)) {
        return std::unexpected(std::format("\"{}\" is a directory, not a ROM file", name));
    }
    if (!std::filesystem::is_regular_file(status)) {
        return std::unexpected(std::format("\"{}\" is not a regular file", name));
    }

    const auto size = std::filesystem::file_size(file, ec);
    if (ec) {
        return std::unexpected(std::format("Cannot read the size of \"{}\": {}", name, ec.message()));
    }
    if (size == 0) {
        return std::unexpected(std::format("The file \"{}\" is empty", name));
    }
    if (size > max_size) {
        return std::unexpected(std::format("The file \"{}\" is too large: {} bytes, but only {} bytes fit", name, size, max_size));
    }

    auto stream = std::ifstream{file, std::ios::binary};
    if (!stream) {
        return std::unexpected(std::format("Cannot open \"{}\"", name));
    }

    auto data = std::vector<uint8_t>(static_cast<size_t>(size));
    stream.read(reinterpret_cast<char*>(data.data()), static_cast<std::streamsize>(data.size()));

    if (static_cast<size_t>(stream.gcount()) != data.size()) {
        return std::unexpected(std::format("Cannot read all of \"{}\"", name));
    }

    return data;
}

} // namespace c8
