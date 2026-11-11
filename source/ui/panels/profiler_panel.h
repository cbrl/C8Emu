#pragma once

#include <cstdint>
#include <vector>

#include "ui/panel.h"

namespace c8 {

/**
 * @class ProfilerPanel
 *
 * @brief The hot spots of the program, and a heat map of executions, reads, and writes
 *
 * @details The hot spots are the addresses that run most often. The heat map
 *          shows all memory as cells: red for executions, green for writes,
 *          and blue for reads. A load or a reset clears the counts.
 *
 *          A click on a hot spot or on a cell shows the code in the Code
 *          Editor. A right click opens the address menu.
 */
class ProfilerPanel final : public Panel {
public:
    [[nodiscard]]
    auto title() const -> const char* override {
        return "Profiler";
    }

    auto draw(UiContext& context) -> void override;

private:
    struct HotSpot {
        uint32_t address = 0;
        uint64_t count   = 0;
    };

    auto draw_hot_spots(UiContext& context) -> void;
    auto draw_heat_map(UiContext& context) -> void;

    std::vector<HotSpot> hot_spots;
    uint64_t total_executions = 0;
    int frames_until_update = 0;

    // The address of the heat map cell that the user right-clicked, for the address menu
    uint32_t menu_address = 0;
};

} // namespace c8
