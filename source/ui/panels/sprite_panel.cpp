#include "sprite_panel.h"

#include "app/source_document.h"
#include "chip8/chip8.h"
#include "debugger/address_names.h"
#include "debugger/debug_expression.h"
#include "debugger/debugger.h"
#include "ui/dock_layout.h"

#include <algorithm>
#include <cmath>
#include <format>

#include <imgui_stdlib.h>

namespace c8 {

namespace {

// The memory bit of one pixel
struct PixelBit {
    uint32_t address;
    uint8_t  mask;
};

auto pixel_bit(const MachineState& state, uint32_t sprite, int row, int column, uint32_t bytes_per_row) -> PixelBit {
    const uint32_t byte = sprite + (static_cast<uint32_t>(row) * bytes_per_row) + static_cast<uint32_t>(column / 8);
    return {.address = byte & state.mask(), .mask = static_cast<uint8_t>(0x80u >> (column % 8))};
}


// The rows as assembler source, for example "    db %.##....."
auto sprite_source(const MachineState& state, uint32_t sprite, int row_count, uint32_t bytes_per_row) -> std::string {
    auto source = std::string{};
    for (int row = 0; row < row_count; ++row) {
        source += "    db";
        for (uint32_t index = 0; index < bytes_per_row; ++index) {
            const uint8_t byte = state.read(sprite + (static_cast<uint32_t>(row) * bytes_per_row) + index);
            source += (index == 0) ? " %" : ", %";
            for (int bit = 7; bit >= 0; --bit) {
                source += (((byte >> bit) & 1) != 0) ? '#' : '.';
            }
        }
        source += '\n';
    }
    return source;
}

} // namespace


auto SpritePanel::draw(UiContext& context) -> void {
    dock_with_on_first_use("Memory");

    if (!ImGui::Begin(title(), &open)) {
        ImGui::End();
        return;
    }

    const auto& state = context.chip.state();

    ImGui::Checkbox("Follow I", &follow_i);
    ImGui::SetItemTooltip("Show the sprite at the I register");
    if (follow_i) {
        address = state.i & state.mask();
    }

    ImGui::SameLine();
    draw_address_field(context);

    ImGui::SameLine();
    ImGui::Checkbox("16x16", &wide);
    ImGui::SetItemTooltip("A sprite of 16 rows of 2 bytes, as DXY0 draws it on SUPER-CHIP and XO-CHIP");

    if (!wide) {
        ImGui::SameLine();
        ImGui::SetNextItemWidth(ImGui::GetFontSize() * 6.0f);
        ImGui::SliderInt("Rows", &rows, 1, 15, "%d", ImGuiSliderFlags_AlwaysClamp);
    }

    if (!address_error.empty()) {
        ImGui::TextColored({1.0f, 0.4f, 0.4f, 1.0f}, "%s", address_error.c_str());
    }

    const int row_count = wide ? 16 : rows;
    const uint32_t bytes_per_row = wide ? 2 : 1;
    const uint32_t last = (address + (static_cast<uint32_t>(row_count) * bytes_per_row) - 1) & state.mask();

    ImGui::Text("0x%04X-0x%04X", address, last);
    if (const auto name = address_name(address, context.document.program_labels()); !name.empty()) {
        ImGui::SameLine();
        ImGui::TextDisabled("(%s)", name.c_str());
    }

    draw_grid(context, row_count, bytes_per_row);

    const auto source = sprite_source(state, address, row_count, bytes_per_row);
    if (ImGui::Button("Copy as db")) {
        ImGui::SetClipboardText(source.c_str());
    }
    ImGui::SetItemTooltip("Copy the rows as assembler source");

    ImGui::SameLine();
    if (ImGui::Button("Show in Memory")) {
        context.requests.show_memory = address;
    }

    ImGui::TextDisabled("%s", source.c_str());

    ImGui::End();
}


auto SpritePanel::draw_address_field(UiContext& context) -> void {
    const auto& state = context.chip.state();

    ImGui::BeginDisabled(follow_i);
    ImGui::SetNextItemWidth(ImGui::GetFontSize() * 10.0f);

    const auto hint = std::format("0x{:04X}", address);
    if (ImGui::InputTextWithHint("##address", hint.c_str(), &address_text, ImGuiInputTextFlags_EnterReturnsTrue)) {
        const auto expression = DebugExpression::compile(address_text, context.debugger.symbols());
        if (expression) {
            address = static_cast<uint32_t>(expression->evaluate(state)) & state.mask();
            address_text.clear();
            address_error.clear();
        }
        else {
            address_error = expression.error();
        }
    }

    ImGui::EndDisabled();
    ImGui::SetItemTooltip("An address, a label, or a debugger expression, for example ball+5. Press Enter.");
}


auto SpritePanel::draw_grid(UiContext& context, int row_count, uint32_t bytes_per_row) -> void {
    auto& state = context.chip.state();
    const int column_count = static_cast<int>(bytes_per_row) * 8;

    // The grid fills the width of the panel
    const float available = ImGui::GetContentRegionAvail().x;
    const float cell = std::clamp(std::floor(available / static_cast<float>(column_count)), 6.0f, ImGui::GetFontSize() * 2.0f);
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    const ImVec2 size = {cell * static_cast<float>(column_count), cell * static_cast<float>(row_count)};

    ImGui::InvisibleButton("##grid", size);
    const bool hovered = ImGui::IsItemHovered();
    const bool active = ImGui::IsItemActive();

    auto* draw_list = ImGui::GetWindowDrawList();
    const auto on_color = ImGui::GetColorU32(ImGuiCol_Text);
    const auto off_color = ImGui::GetColorU32(ImGuiCol_FrameBg);
    const auto line_color = ImGui::GetColorU32(ImGuiCol_Border);

    for (int row = 0; row < row_count; ++row) {
        for (int column = 0; column < column_count; ++column) {
            const auto bit = pixel_bit(state, address, row, column, bytes_per_row);
            const bool set = (state.read(bit.address) & bit.mask) != 0;

            const ImVec2 a = {origin.x + (static_cast<float>(column) * cell), origin.y + (static_cast<float>(row) * cell)};
            const ImVec2 b = {a.x + cell, a.y + cell};
            draw_list->AddRectFilled(a, b, set ? on_color : off_color);
            draw_list->AddRect(a, b, line_color);
        }
    }

    if (!active) {
        paint_value.reset();
    }
    if (!hovered && !active) {
        return;
    }

    const ImVec2 mouse = ImGui::GetMousePos();
    const auto column = static_cast<int>(std::floor((mouse.x - origin.x) / cell));
    const auto row = static_cast<int>(std::floor((mouse.y - origin.y) / cell));
    if ((row < 0) || (row >= row_count) || (column < 0) || (column >= column_count)) {
        return;
    }

    const auto bit = pixel_bit(state, address, row, column, bytes_per_row);
    if (hovered) {
        ImGui::SetTooltip("0x%04X, bit %d", bit.address, 7 - (column % 8));
    }

    if (active) {
        const uint8_t byte = state.read(bit.address);
        if (!paint_value) {
            paint_value = (byte & bit.mask) == 0;
        }

        const auto painted = static_cast<uint8_t>(*paint_value ? (byte | bit.mask) : (byte & ~bit.mask));
        if (painted != byte) {
            state.write(bit.address, painted);

            // A replay from the history would not give the painted pixels
            context.requests.state_edited = true;
        }
    }
}

} // namespace c8
