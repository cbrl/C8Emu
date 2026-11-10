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


/// The data memory that an instruction reads and writes. An address wraps at the end of memory.
struct MemoryAccess {
    uint32_t read_address  = 0;
    uint32_t read_length   = 0; ///< 0 if the instruction reads no data
    uint32_t write_address = 0;
    uint32_t write_length  = 0; ///< 0 if the instruction writes no data

    [[nodiscard]]
    auto empty() const noexcept -> bool {
        return (read_length == 0) && (write_length == 0);
    }
};

/**
 * @brief The data memory that an instruction will read and write
 *
 * @details The access is for the state before the instruction runs. The
 *          fetch of the instruction words does not count. A draw reads all
 *          rows of the sprite, also rows that the display edge clips.
 *
 * @param[in] state    The machine state before the instruction
 * @param[in] word     The first instruction word
 * @param[in] variant  The instruction set
 */
[[nodiscard]]
auto memory_access(const MachineState& state, uint16_t word, Variant variant) noexcept -> MemoryAccess;


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
