#include "session.h"

#include "chip8/analysis.h"
#include "chip8/chip8.h"
#include "debugger/debugger.h"
#include "frontend/rom_files.h"
#include "util/crc32.h"
#include "util/log.h"

#include <system_error>
#include <vector>

namespace c8 {

Session::Session(Chip8& machine, Debugger& run_control) noexcept : chip(machine), debugger(run_control) {
}


auto Session::open_rom(const std::filesystem::path& file, const Settings& settings) -> bool {
    // XO-CHIP has the largest memory. load() checks the size for the platform.
    const auto data = read_rom_file(file, Chip8::max_program_size(Variant::xochip));
    if (!data) {
        log_error("Cannot load the ROM: {}", data.error());
        return false;
    }

    const auto rom_crc = crc32(*data);
    const auto previous = chip.variant();

    // The configuration that the user chose for this ROM before
    if (const auto it = settings.rom_profiles.find(rom_crc); it != settings.rom_profiles.end()) {
        const auto& profile = it->second;
        if (const auto loaded = debugger.load_program(*data, MachineState::program_start, profile.variant); !loaded) {
            log_error("Cannot load the ROM: {}", loaded.error());
            return false;
        }
        chip.set_quirks(profile.quirks);
        chip.set_instructions_per_frame(profile.instructions_per_frame);

        loaded_rom(file, rom_crc);
        log_info("Machine: the saved settings of this ROM ({}, {} instructions per frame)",
            to_string(profile.variant), chip.instructions_per_frame());
        return true;
    }

    // The extension can name the platform. If not, the code can show it.
    auto variant = chip.variant();
    auto reason = std::string{};

    if (const auto named = platform_from_extension(file)) {
        variant = *named;
        reason = "from the file extension";
    }
    else if (settings.auto_detect_platform) {
        auto guess = detect_platform(*data, MachineState::program_start);
        variant = guess.variant;
        reason = std::move(guess.reason);
    }

    if (const auto loaded = load(*data, variant); !loaded) {
        log_error("Cannot load the ROM: {}", loaded.error());
        return false;
    }

    loaded_rom(file, rom_crc);
    if (variant != previous) {
        log_info("Platform: {} ({})", to_string(variant), reason);
    }
    return true;
}


auto Session::loaded_rom(const std::filesystem::path& file, uint32_t rom_crc) -> void {
    rom_file = file;
    name = path_to_utf8(file.filename());
    crc = rom_crc;
    known_profile = current_profile();

    log_info("Loaded \"{}\" ({} bytes)", name, chip.program().size());
}


auto Session::load_assembled(std::span<const uint8_t> program, Variant variant, std::string_view source_name) -> bool {
    const auto previous = chip.variant();
    if (const auto loaded = load(program, variant); !loaded) {
        log_error("Cannot load the assembled program: {}", loaded.error());
        return false;
    }

    name = source_name;
    crc.reset();
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


auto Session::state_loaded(std::string_view state_name, bool same_program) -> void {
    if (!state_name.empty()) {
        name = state_name;
    }
    if (!same_program) {
        crc.reset();
    }

    // The configuration of the save state is not a choice of the user for the ROM
    known_profile = current_profile();
}


auto Session::take_profile_change() -> std::optional<std::pair<uint32_t, RomProfile>> {
    if (!crc) {
        return std::nullopt;
    }

    const auto profile = current_profile();
    if (profile == known_profile) {
        return std::nullopt;
    }

    known_profile = profile;
    return std::pair{*crc, profile};
}


auto Session::current_profile() const noexcept -> RomProfile {
    return RomProfile{
        .variant = chip.variant(),
        .quirks  = chip.quirks(),
        .instructions_per_frame = chip.instructions_per_frame(),
    };
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
