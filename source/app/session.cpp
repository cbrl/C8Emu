#include "session.h"

#include "chip8/chip8.h"
#include "debugger/debugger.h"
#include "frontend/rom_files.h"
#include "util/log.h"

#include <system_error>
#include <vector>

namespace c8 {

Session::Session(Chip8& machine, Debugger& run_control) noexcept : chip(machine), debugger(run_control) {
}


auto Session::open_rom(const std::filesystem::path& file) -> bool {
    const auto loaded = read_rom_file(file, chip.max_program_size())
        .and_then([this](const std::vector<uint8_t>& data) { return debugger.load_program(data); });

    if (!loaded) {
        log_error("Cannot load the ROM: {}", loaded.error());
        return false;
    }

    rom_file = file;
    name = path_to_utf8(file.filename());

    log_info("Loaded \"{}\" ({} bytes)", name, chip.program().size());
    return true;
}


auto Session::load_assembled(std::span<const uint8_t> program, std::string_view source_name) -> bool {
    if (const auto loaded = debugger.load_program(program); !loaded) {
        log_error("Cannot load the assembled program: {}", loaded.error());
        return false;
    }

    name = source_name;
    log_info("Assembled and loaded \"{}\" ({} bytes)", name, program.size());
    return true;
}


auto Session::reset() -> void {
    debugger.reset();

    if (!name.empty()) {
        log_info("Reset \"{}\"", name);
    }
}


auto Session::browse_folder() const -> std::filesystem::path {
    auto ec = std::error_code{};

    if (!rom_file.empty()) {
        return rom_file.parent_path();
    }

    // Start in the bundled ROM folder if it exists
    const auto roms = std::filesystem::current_path(ec) / "roms";
    if (!ec && std::filesystem::is_directory(roms, ec)) {
        return roms;
    }

    return std::filesystem::current_path(ec);
}

} // namespace c8
