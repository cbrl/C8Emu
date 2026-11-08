#pragma once

#include <cstdint>

#include "ui/panel.h"

namespace c8 {

/**
 * @class ProgramPanel
 *
 * @brief A listing of all memory as instructions
 *
 * @details Each row is one instruction. The rows start at an even or an odd
 *          address, the same as the PC, so the PC row always decodes
 *          correctly. A click on the dot of a row toggles a breakpoint.
 */
class ProgramPanel final : public Panel {
public:
    [[nodiscard]]
    auto title() const -> const char* override {
        return "Program";
    }

    auto draw(UiContext& context) -> void override;

private:
    // Scroll to the PC row when the PC changes and the row is not visible
    bool follow_pc = true;

    // The PC in the last frame. A different PC scrolls the listing if follow_pc is on.
    uint32_t shown_pc = 0x10000;
};

} // namespace c8
