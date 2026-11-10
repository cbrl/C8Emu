#include "session.h"

#include "chip8/analysis.h"
#include "chip8/chip8.h"
#include "debugger/debugger.h"
#include "frontend/rom_files.h"
#include "util/log.h"

#include <system_error>
#include <vector>

namespace c8 {

Session::Session(Chip8& machine, Debugger& run_control) noexcept : chip(machine), debugger(run_control) {
}


auto Session::open_rom(const std::filesystem::path& file, bool auto_detect) -> bool {
    // XO-CHIP has the largest memory. load() checks the size for the platform.
    const auto data = read_rom_file(file, Chip8::max_program_size(Variant::xochip));
    if (!data) {
        log_error("Cannot load the ROM: {}", data.error());
        return false;
    }

    // The extension can name the platform. If not, the code can show it.
    auto variant = chip.variant();
    auto reason = std::string{};

    if (const auto named = platform_from_extension(file)) {
        variant = *named;
        reason = "from the file extension";
    }
    else if (auto_detect) {
        auto guess = detect_platform(*data, MachineState::program_start);
        variant = guess.variant;
        reason = std::move(guess.reason);
    }

    const auto previous = chip.variant();
    if (const auto loaded = load(*data, variant); !loaded) {
        log_error("Cannot load the ROM: {}", loaded.error());
        return false;
    }

    rom_file = file;
    name = path_to_utf8(file.filename());

    log_info("Loaded \"{}\" ({} bytes)", name, chip.program().size());
    if (variant != previous) {
        log_info("Platform: {} ({})", to_string(variant), reason);
    }
    return true;
}


auto Session::load_assembled(std::span<const uint8_t> program, Variant variant, std::string_view source_name) -> bool {
    const auto previous = chip.variant();
    if (const auto loaded = load(program, variant); !loaded) {
        log_error("Cannot load the assembled program: {}", loaded.error());
        return false;
    }

    name = source_name;
    log_info("Assembled and loaded \"{}\" ({} bytes)", name, program.size());
    if (variant != previous) {
        log_info("Platform: {} (from the platform line)", to_string(variant));
    }
    return true;
}


auto Session::load(std::span<const uint8_t> program, Variant variant) -> std::expected<void, std::string> {
    const auto* preset = (variant != chip.variant()) ? &default_preset(variant) : nullptr;

    auto loaded = debugger.load_program(program, MachineState::program_start, variant);
    if (loaded && (preset != nullptr)) {
        chip.set_quirks(preset->quirks);
        chip.set_instructions_per_frame(preset->instructions_per_frame);
    }
    return loaded;
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
