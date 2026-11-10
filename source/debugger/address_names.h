#pragma once

#include <cstdint>
#include <span>
#include <string>

#include "assembler/assembler.h"

namespace c8 {

/**
 * @brief The name of an address from the labels of the program
 *
 * @details The name uses the last label at or before the address: "loop" for
 *          the address of the label, "loop+0x4" for an address after it.
 *
 * @param[in] address  The address
 * @param[in] labels   The labels, in order of address (see SourceDocument::program_labels())
 *
 * @return The name, or an empty text if no label is at or before the address
 */
[[nodiscard]]
auto address_name(uint32_t address, std::span<const Label> labels) -> std::string;

} // namespace c8
