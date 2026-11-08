#include "program_panel.h"

#include "chip8/chip8.h"
#include "instruction/instruction.h"

#include <algorithm>

namespace c8 {

auto ProgramPanel::draw(UiContext& context) -> void {
    if (ImGui::Begin(title(), &open, ImGuiWindowFlags_NoScrollbar)) {
        const auto& state = context.chip.state();

        ImGui::Text("Execution");
        ImGui::Separator();

        ImGui::InputInt("Preview Count", &instruction_count);
        instruction_count = std::clamp(instruction_count, 1, 64);
        ImGui::Separator();

        // Decode the instructions from the PC forward. Addresses wrap at the end of memory.
        const auto count = static_cast<size_t>(instruction_count);
        instructions.resize(count);

        for (size_t i = 0; i < count; ++i) {
            const auto address = static_cast<uint16_t>((state.pc + (2 * i)) & state.mask());
            instructions[i] = to_string(instruction{state.read_word(address)});

            if (i == 0) {
                ImGui::Text("0x%04X - %s", address, instructions[i].c_str());
            }
            else {
                ImGui::TextDisabled("0x%04X - %s", address, instructions[i].c_str());
            }
        }
    }
    ImGui::End();
}

} // namespace c8
