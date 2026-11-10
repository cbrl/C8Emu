#include "settings_store.h"

#include "frontend/rom_files.h"

#include <glaze/json.hpp>

#include <format>
#include <system_error>
#include <utility>

// Glaze writes each member of Settings with the name of the member as the key.
// These enumerations are written as names, and a read fails on an unknown name.

template<>
struct glz::meta<c8::Variant> {
    using enum c8::Variant;
    static constexpr auto value = glz::enumerate("chip8", chip8, "hires", chip8_hires, "schip", schip, "xochip", xochip);
};

template<>
struct glz::meta<c8::LoadStoreQuirk> {
    using enum c8::LoadStoreQuirk;
    static constexpr auto value = glz::enumerate(increment_x_plus_1, increment_x, unchanged);
};

template<>
struct glz::meta<c8::DisassemblyMode> {
    using enum c8::DisassemblyMode;
    static constexpr auto value = glz::enumerate(flow, linear);
};

namespace c8 {

namespace {

constexpr auto read_options = glz::opts{.error_on_unknown_keys = false};
constexpr auto write_options = glz::opts{.prettify = true};

} // namespace


auto read_settings_file(const std::filesystem::path& file) -> std::expected<Settings, std::string> {
    auto ec = std::error_code{};
    if (!std::filesystem::exists(file, ec)) {
        return Settings{};
    }

    const auto text = read_text_file(file);
    if (!text) {
        return std::unexpected(text.error());
    }

    auto settings = Settings{};
    if (const auto error = glz::read<read_options>(settings, *text)) {
        return std::unexpected(std::format("\"{}\" is not valid: {}", path_to_utf8(file), glz::format_error(error, *text)));
    }

    settings.clamp();
    return settings;
}


auto write_settings_file(const std::filesystem::path& file, const Settings& settings) -> std::expected<void, std::string> {
    auto text = std::string{};
    if (const auto error = glz::write<write_options>(settings, text)) {
        return std::unexpected(std::format("Cannot write the settings: {}", glz::format_error(error)));
    }

    auto temporary = file;
    temporary += ".tmp";

    if (auto written = write_text_file(temporary, text); !written) {
        return written;
    }

    auto ec = std::error_code{};
    std::filesystem::rename(temporary, file, ec);
    if (ec) {
        auto message = std::format("Cannot replace \"{}\": {}", path_to_utf8(file), ec.message());
        std::filesystem::remove(temporary, ec);
        return std::unexpected(std::move(message));
    }

    return {};
}

} // namespace c8
