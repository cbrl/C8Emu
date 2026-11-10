#pragma once

#include <cstdint>

#include "chip8/machine_state.h"
#include "chip8/quirks.h"
#include "chip8/variant.h"

namespace c8::isa {

/// A side effect of an instruction that the scheduler must know about
enum class Effect : uint8_t {
    none,
    drew,    ///< DXYN changed the display
    waiting, ///< FX0A waits for a key. The PC did not advance. MachineState::key_wait has the progress.
    exited,  ///< 00FD stopped the program. The PC did not advance.
};

struct ExecResult {
    Effect effect = Effect::none;
    Fault  fault  = Fault::none;
};


/**
 * @brief Execute one instruction
 *
 * @details The function decodes the opcode for the variant, changes the
 *          state, and advances the PC. If the instruction faults, the state
 *          does not change. F000 NNNN reads its second word from PC + 2.
 *
 * @param[in,out] state    The machine state
 * @param[in]     quirks   The interpreter behaviors to use
 * @param[in]     variant  The instruction set
 * @param[in]     opcode   The 16-bit instruction word at the PC
 */
[[nodiscard]]
auto execute(MachineState& state, const Quirks& quirks, Variant variant, uint16_t opcode) noexcept -> ExecResult;

} // namespace c8::isa
