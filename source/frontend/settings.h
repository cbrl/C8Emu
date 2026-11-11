#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <map>
#include <string>
#include <vector>

#include "assembler/disassembler.h"
#include "chip8/quirks.h"
#include "chip8/variant.h"

namespace c8 {

/**
 * @brief The display colors as RGBA values, indexed by the plane bits of a pixel
 *
 * @details Index 0 is an off pixel and index 1 is a pixel in plane 1. Only
 *          XO-CHIP uses index 2 (plane 2) and index 3 (both planes).
 */
using Palette = std::array<uint32_t, 4>;


/// The machine configuration that the user chose for one ROM
struct RomProfile {
    Variant  variant = Variant::chip8;
    Quirks   quirks;
    uint32_t instructions_per_frame = 15;

    friend auto operator==(const RomProfile&, const RomProfile&) -> bool = default;
};


/**
 * @struct Settings
 *
 * @brief The frontend settings
 *
 * @details The settings file holds these members (app/settings_store). Glaze
 *          reads and writes the struct by reflection, so it must stay an
 *          aggregate. The name of a member is its key in the file. A rename
 *          of a member therefore changes the file.
 */
struct Settings {
    // The range of the host frame limit. The emulated frame rate is fixed at c8::Chip8::frame_rate (60).
    // The main loop catches up at most 4 emulated frames, so a host rate below 15 FPS slows the emulation.
    // The minimum of 30 keeps a margin above that.
    static constexpr uint32_t min_frame_limit = 30;
    static constexpr uint32_t max_frame_limit = 1000;

    // While the turbo key is held, the emulation runs this many times faster
    static constexpr uint32_t min_turbo_factor = 2;
    static constexpr uint32_t max_turbo_factor = 16;

    // The length of the history for step back and rewind
    static constexpr uint32_t min_history_seconds = 5;
    static constexpr uint32_t max_history_seconds = 600;

    // The quick save slots are 1 to state_slot_count
    static constexpr uint32_t state_slot_count = 9;

    // The longest lists of recent files
    static constexpr size_t max_recent_files = 10;

    // Display
    Palette  palette{0x000000FF, 0xFFFFFFFF, 0xAAAAAAFF, 0x555555FF};
    bool     frame_limit_enabled = true;
    uint32_t frame_limit         = 60;      ///< Frames per second

    // Audio
    float volume = 0.5f;                    ///< [0, 1]
    bool  muted  = false;

    // Emulation
    uint32_t turbo_factor = 4;              ///< The speed multiplier while the turbo key is held

    /// Open ROM selects the platform from the code of the program, if the file extension names none
    bool auto_detect_platform = true;

    bool     history_enabled = true;        ///< Record the frames for step back and rewind
    uint32_t history_seconds = 60;          ///< The length of the history
    uint32_t state_slot      = 1;           ///< The slot of quick save and quick load

    // The machine of the last session
    Variant  variant = Variant::chip8;
    Quirks   quirks;
    uint32_t instructions_per_frame = 15;
    bool     break_on_leave_program = true;

    // Code editor
    DisassemblyMode disassembly_mode = DisassemblyMode::flow; ///< How the code editor disassembles a ROM
    bool editor_line_numbers = true;
    bool editor_whitespace   = true;
    bool editor_mini_map     = false;

    /// The titles of the panels that the user closed
    std::vector<std::string> closed_panels;

    /// The expressions of the Watch panel, in the order of the panel
    std::vector<std::string> watches;

    // Files. UTF-8 paths, newest first.
    std::vector<std::string> recent_roms;
    std::vector<std::string> recent_sources;

    /// The configuration of each ROM whose platform, quirks, or speed the user changed, by the CRC-32 of the ROM
    std::map<uint32_t, RomProfile> rom_profiles;

    friend auto operator==(const Settings&, const Settings&) -> bool = default;

    /// Clamp each value to its range, and cut the lists of recent files to their largest length
    auto clamp() -> void {
        frame_limit     = std::clamp(frame_limit, min_frame_limit, max_frame_limit);
        turbo_factor    = std::clamp(turbo_factor, min_turbo_factor, max_turbo_factor);
        volume          = (volume > 0.0f) ? std::min(volume, 1.0f) : 0.0f;
        history_seconds = std::clamp(history_seconds, min_history_seconds, max_history_seconds);
        state_slot      = std::clamp(state_slot, 1u, state_slot_count);

        for (auto* list : {&recent_roms, &recent_sources}) {
            if (list->size() > max_recent_files) {
                list->resize(max_recent_files);
            }
        }
    }

    /// Put a file at the front of a list of recent files. The oldest files leave a full list.
    static auto push_recent(std::vector<std::string>& list, const std::string& path) -> void {
        std::erase(list, path);
        list.insert(list.begin(), path);
        if (list.size() > max_recent_files) {
            list.resize(max_recent_files);
        }
    }
};

} // namespace c8
