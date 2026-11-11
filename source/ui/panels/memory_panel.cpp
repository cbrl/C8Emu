#include "memory_panel.h"

#include "chip8/chip8.h"
#include "debugger/debugger.h"

namespace c8 {

namespace {

constexpr ImU32 pc_color         = IM_COL32(255, 200, 0, 120);
constexpr ImU32 breakpoint_color = IM_COL32(255, 64, 64, 120);
constexpr ImU32 changed_color    = IM_COL32(255, 140, 64, 90);
constexpr ImU32 watch_color      = IM_COL32(190, 110, 255, 80);
constexpr ImU32 i_color          = IM_COL32(64, 160, 255, 120);
constexpr ImU32 program_color    = IM_COL32(255, 255, 255, 20);
constexpr ImU32 font_color       = IM_COL32(120, 255, 120, 30);

// 16 digit sprites, 5 bytes each
constexpr size_t font_size = 16 * 5;


// A colored square and a label, for the legend
auto legend_item(ImU32 color, const char* label) -> void {
    const float size = ImGui::GetTextLineHeight();
    const ImVec2 pos = ImGui::GetCursorScreenPos();

    ImGui::GetWindowDrawList()->AddRectFilled(pos, {pos.x + size, pos.y + size}, color);
    ImGui::Dummy({size, size});
    ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
    ImGui::TextUnformatted(label);
}

} // namespace


MemoryPanel::MemoryPanel() {
    editor.BgColorFn = &MemoryPanel::background_color;
    editor.WriteFn = &MemoryPanel::write_byte;
    editor.UserData = &data;
}


auto MemoryPanel::draw(UiContext& context) -> void {
    apply_focus_request();

    if (!ImGui::Begin(title(), &open)) {
        ImGui::End();
        return;
    }

    auto& state = context.chip.state();

    if (ImGui::Button("Go to PC")) {
        go_to(state.pc);
    }
    ImGui::SameLine();
    if (ImGui::Button("Go to I")) {
        go_to(state.i & state.mask());
    }

    ImGui::SameLine();
    ImGui::Spacing();
    ImGui::SameLine();
    legend_item(pc_color, "PC");
    ImGui::SameLine();
    legend_item(i_color, "I");
    ImGui::SameLine();
    legend_item(breakpoint_color, "Breakpoint");
    ImGui::SameLine();
    legend_item(watch_color, "Watchpoint");
    ImGui::SameLine();
    legend_item(changed_color, "Changed");
    ImGui::SetItemTooltip("The bytes that changed since the stop before");
    ImGui::SameLine();
    legend_item(program_color, "Program");
    ImGui::SameLine();
    legend_item(font_color, "Font");

    data = EditorData{
        .state         = &state,
        .previous      = context.previous_stop,
        .debugger      = &context.debugger,
        .program_begin = context.chip.program_origin(),
        .program_end   = context.chip.program_end(),
    };

    editor.DrawContents(state.memory.data(), state.memory.size());

    // A replay from the history would not give the edited bytes
    if (data.edited) {
        context.requests.state_edited = true;
    }

    if (jumped) {
        // A jump also selects the byte for editing, and the edit field takes the keyboard.
        // Only scroll, so the CHIP-8 keys keep working.
        editor.DataEditingAddr = static_cast<size_t>(-1);
        editor.DataEditingTakeFocus = false;
        jumped = false;
    }

    ImGui::End();
}


auto MemoryPanel::background_color(const ImU8* /*memory*/, size_t address, void* user_data) -> ImU32 {
    const auto& source = *static_cast<const EditorData*>(user_data);
    if (source.state == nullptr) {
        return 0;
    }

    const auto& state = *source.state;

    // An instruction is two bytes, so mark both bytes of the PC and of each breakpoint
    if ((address == state.pc) || (address == ((state.pc + 1u) & state.mask()))) {
        return pc_color;
    }

    const auto& breakpoints = source.debugger->breakpoints();
    const auto previous = static_cast<uint16_t>((address - 1) & state.mask());
    if (breakpoints.contains(static_cast<uint16_t>(address)) || breakpoints.contains(previous)) {
        return breakpoint_color;
    }

    // A change of platform in this frame changes the memory size, so check the size of the copy
    const auto* previous_memory = (source.previous != nullptr) ? &source.previous->memory : nullptr;
    if ((previous_memory != nullptr) && (address < previous_memory->size()) && ((*previous_memory)[address] != state.memory[address])) {
        return changed_color;
    }

    for (const auto& watch : source.debugger->watchpoints()) {
        if (watch.enabled && (address >= watch.address) && (address < (uint32_t{watch.address} + watch.length))) {
            return watch_color;
        }
    }

    if (address == (state.i & state.mask())) {
        return i_color;
    }

    // An address below the font wraps to a large offset
    if ((address - MachineState::font_address) < font_size) {
        return font_color;
    }

    if ((address >= source.program_begin) && (address < source.program_end)) {
        return program_color;
    }

    return 0;
}


auto MemoryPanel::write_byte(ImU8* memory, size_t address, ImU8 value, void* user_data) -> void {
    memory[address] = value;
    static_cast<EditorData*>(user_data)->edited = true;
}


auto MemoryPanel::show(size_t address) -> void {
    bring_to_front();
    go_to(address);
}


auto MemoryPanel::go_to(size_t address) -> void {
    editor.GotoAddr = address;
    jumped = true;
}

} // namespace c8
