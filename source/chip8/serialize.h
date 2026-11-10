#pragma once

#include <cstdint>
#include <expected>
#include <span>
#include <string>
#include <vector>

#include "chip8/chip8.h"
#include "chip8/machine_state.h"

namespace c8 {

/**
 * @brief Write a machine state as bytes
 *
 * @details The bytes replace the contents of `out`, so a caller can use the
 *          same buffer again without a new allocation. The format has no
 *          header. The history uses it, and a save state file holds it.
 */
auto serialize_state(const MachineState& state, std::vector<uint8_t>& out) -> void;

/// Write a machine state as bytes
[[nodiscard]]
auto serialize_state(const MachineState& state) -> std::vector<uint8_t>;

/**
 * @brief Read a machine state from bytes
 *
 * @details The function checks the format: the sizes, the values of the
 *          enumerations, and the length of the data. Chip8::load_state()
 *          checks that the state fits a variant.
 *
 * @return The state, or an error message
 */
[[nodiscard]]
auto deserialize_state(std::span<const uint8_t> bytes) -> std::expected<MachineState, std::string>;


/**
 * @brief Write a save state file
 *
 * @details The file starts with a magic text and a version number. A file of
 *          another version does not load.
 */
[[nodiscard]]
auto serialize_save_state(const SaveState& save) -> std::vector<uint8_t>;

/// Read a save state file. Returns an error message for a file of another version and for bad data.
[[nodiscard]]
auto deserialize_save_state(std::span<const uint8_t> bytes) -> std::expected<SaveState, std::string>;

} // namespace c8
