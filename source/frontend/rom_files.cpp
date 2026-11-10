#include "rom_files.h"

#include <algorithm>
#include <format>
#include <fstream>
#include <system_error>

namespace c8 {

namespace {

// Read a whole file. `what` names the kind of file in the error messages, for example "ROM file".
auto read_file(const std::filesystem::path& file, size_t max_size, bool allow_empty, std::string_view what)
    -> std::expected<std::vector<uint8_t>, std::string> {

    const auto name = path_to_utf8(file);
    auto ec = std::error_code{};

    const auto status = std::filesystem::status(file, ec);
    if (ec || !std::filesystem::exists(status)) {
        return std::unexpected(std::format("The file \"{}\" does not exist", name));
    }
    if (std::filesystem::is_directory(status)) {
        return std::unexpected(std::format("\"{}\" is a directory, not a {}", name, what));
    }
    if (!std::filesystem::is_regular_file(status)) {
        return std::unexpected(std::format("\"{}\" is not a regular file", name));
    }

    const auto size = std::filesystem::file_size(file, ec);
    if (ec) {
        return std::unexpected(std::format("Cannot read the size of \"{}\": {}", name, ec.message()));
    }
    if ((size == 0) && !allow_empty) {
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

} // namespace


auto path_to_utf8(const std::filesystem::path& path) -> std::string {
    const auto text = path.u8string();
    return std::string{reinterpret_cast<const char*>(text.data()), text.size()};
}


auto path_from_utf8(std::string_view text) -> std::filesystem::path {
    const auto utf8 = std::u8string{reinterpret_cast<const char8_t*>(text.data()), text.size()};
    return std::filesystem::path{utf8};
}


auto platform_from_extension(const std::filesystem::path& file) -> std::optional<Variant> {
    auto extension = path_to_utf8(file.extension());
    std::ranges::transform(extension, extension.begin(), [](char c) {
        return ((c >= 'A') && (c <= 'Z')) ? static_cast<char>(c - 'A' + 'a') : c;
    });

    if (extension == ".c8h") {
        return Variant::chip8_hires;
    }
    if (extension == ".sc8") {
        return Variant::schip;
    }
    if (extension == ".xo8") {
        return Variant::xochip;
    }
    return std::nullopt;
}


auto read_rom_file(const std::filesystem::path& file, size_t max_size) -> std::expected<std::vector<uint8_t>, std::string> {
    return read_file(file, max_size, false, "ROM file");
}


auto read_text_file(const std::filesystem::path& file, size_t max_size) -> std::expected<std::string, std::string> {
    return read_file(file, max_size, true, "text file").transform([](const std::vector<uint8_t>& data) {
        auto text = std::string{reinterpret_cast<const char*>(data.data()), data.size()};

        if (text.starts_with("\xEF\xBB\xBF")) {
            text.erase(0, 3);
        }
        return text;
    });
}


auto write_file(const std::filesystem::path& file, std::span<const uint8_t> data) -> std::expected<void, std::string> {
    const auto name = path_to_utf8(file);

    auto stream = std::ofstream{file, std::ios::binary | std::ios::trunc};
    if (!stream) {
        return std::unexpected(std::format("Cannot open \"{}\" for writing", name));
    }

    stream.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
    stream.close();

    if (!stream) {
        return std::unexpected(std::format("Cannot write all of \"{}\"", name));
    }
    return {};
}


auto write_text_file(const std::filesystem::path& file, std::string_view text) -> std::expected<void, std::string> {
    const auto bytes = std::span{reinterpret_cast<const uint8_t*>(text.data()), text.size()};
    return write_file(file, bytes);
}

} // namespace c8
