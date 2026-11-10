#include "profiler_panel.h"

#include "app/source_document.h"
#include "assembler/disassembler.h"
#include "chip8/chip8.h"
#include "debugger/address_names.h"
#include "debugger/debugger.h"
#include "ui/dock_layout.h"

#include <algorithm>
#include <cmath>
#include <format>
#include <span>
#include <string>

namespace c8 {

namespace {

constexpr size_t hot_spot_count = 100;

// The hot spots update this often, because a sort of all memory in each frame costs too much
constexpr int update_interval = 15;

constexpr size_t heat_map_columns = 64;
constexpr size_t heat_map_cells   = 4096;

constexpr auto label_color = ImVec4{0.35f, 0.70f, 0.61f, 1.0f};


// 0 for no activity, then a logarithmic scale up to 1 for the maximum
auto intensity(uint64_t value, uint64_t max) -> float {
    if ((value == 0) || (max == 0)) {
        return 0.0f;
    }
    return 0.25f + (0.75f * static_cast<float>(std::log1p(static_cast<double>(value)) / std::log1p(static_cast<double>(max))));
}

} // namespace


auto ProfilerPanel::draw(UiContext& context) -> void {
    dock_with_on_first_use("Log");

    if (!ImGui::Begin(title(), &open)) {
        ImGui::End();
        return;
    }

    auto& debugger = context.debugger;
    const auto& profiler = debugger.profiler();

    bool profiling = profiler.is_running();
    if (ImGui::Checkbox("Profile", &profiling)) {
        debugger.set_profiling(profiling);
        frames_until_update = 0;
    }
    ImGui::SetItemTooltip("Count the executions of each address and the reads and writes of each byte.\n"
                          "A load or a reset clears the counts.");

    ImGui::SameLine();
    if (ImGui::Button("Clear")) {
        debugger.clear_profile();
        frames_until_update = 0;
    }

    if (profiler.execution_counts().empty()) {
        ImGui::TextDisabled("Turn on Profile, then run the program.");
        ImGui::End();
        return;
    }

    if (ImGui::BeginTabBar("##views")) {
        if (ImGui::BeginTabItem("Hot Spots")) {
            draw_hot_spots(context);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Heat Map")) {
            draw_heat_map(context);
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }

    ImGui::End();
}


auto ProfilerPanel::draw_hot_spots(UiContext& context) -> void {
    const auto counts = context.debugger.profiler().execution_counts();

    if (--frames_until_update <= 0) {
        frames_until_update = update_interval;

        hot_spots.clear();
        total_executions = 0;
        for (size_t address = 0; address < counts.size(); ++address) {
            if (counts[address] > 0) {
                hot_spots.push_back(HotSpot{.address = static_cast<uint32_t>(address), .count = counts[address]});
                total_executions += counts[address];
            }
        }

        const auto keep = std::min(hot_spots.size(), hot_spot_count);
        std::ranges::partial_sort(hot_spots, hot_spots.begin() + static_cast<std::ptrdiff_t>(keep), std::ranges::greater{}, &HotSpot::count);
        hot_spots.resize(keep);
    }

    ImGui::TextDisabled("%llu instructions counted", static_cast<unsigned long long>(total_executions));

    const auto& state = context.chip.state();
    const auto& labels = context.document.program_labels();

    constexpr auto table_flags = ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_Resizable;
    if (!ImGui::BeginTable("##hot", 5, table_flags)) {
        return;
    }

    ImGui::TableSetupScrollFreeze(0, 1);
    ImGui::TableSetupColumn("Address", ImGuiTableColumnFlags_WidthFixed);
    ImGui::TableSetupColumn("Location", ImGuiTableColumnFlags_WidthFixed);
    ImGui::TableSetupColumn("Executions", ImGuiTableColumnFlags_WidthFixed);
    ImGui::TableSetupColumn("Share", ImGuiTableColumnFlags_WidthFixed);
    ImGui::TableSetupColumn("Instruction", ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableHeadersRow();

    for (const auto& spot : hot_spots) {
        ImGui::PushID(static_cast<int>(spot.address));
        ImGui::TableNextRow();

        ImGui::TableNextColumn();
        if (ImGui::Selectable(std::format("0x{:04X}", spot.address).c_str(), false, ImGuiSelectableFlags_SpanAllColumns)) {
            context.document.reveal(static_cast<uint16_t>(spot.address));
        }

        ImGui::TableNextColumn();
        ImGui::TextColored(label_color, "%s", address_name(spot.address, labels).c_str());

        ImGui::TableNextColumn();
        ImGui::Text("%llu", static_cast<unsigned long long>(spot.count));

        ImGui::TableNextColumn();
        const float share = (total_executions > 0) ? static_cast<float>(spot.count) / static_cast<float>(total_executions) : 0.0f;
        const auto share_text = std::format("{:.1f}%", share * 100.0f);
        ImGui::ProgressBar(share, {ImGui::GetFontSize() * 5.0f, 0.0f}, share_text.c_str());

        ImGui::TableNextColumn();
        const auto text = format_instruction(state.read_word(spot.address), state.read_word(spot.address + 2), context.chip.variant());
        ImGui::TextUnformatted(text.c_str());

        ImGui::PopID();
    }

    ImGui::EndTable();
}


auto ProfilerPanel::draw_heat_map(UiContext& context) -> void {
    const auto& profiler = context.debugger.profiler();
    const auto executions = profiler.execution_counts();
    const auto reads = profiler.read_counts();
    const auto writes = profiler.write_counts();

    // Each cell is a block of memory
    const size_t cells = std::min(executions.size(), heat_map_cells);
    const size_t block = executions.size() / cells;
    const size_t rows = cells / heat_map_columns;

    const auto block_sum = [block](std::span<const uint64_t> counts, size_t cell) {
        uint64_t sum = 0;
        for (size_t offset = 0; offset < block; ++offset) {
            sum += counts[(cell * block) + offset];
        }
        return sum;
    };

    uint64_t max_executions = 1;
    uint64_t max_reads = 1;
    uint64_t max_writes = 1;
    for (size_t cell = 0; cell < cells; ++cell) {
        max_executions = std::max(max_executions, block_sum(executions, cell));
        max_reads = std::max(max_reads, block_sum(reads, cell));
        max_writes = std::max(max_writes, block_sum(writes, cell));
    }

    ImGui::TextColored({1.0f, 0.35f, 0.35f, 1.0f}, "Executed");
    ImGui::SameLine();
    ImGui::TextColored({0.35f, 1.0f, 0.35f, 1.0f}, "Written");
    ImGui::SameLine();
    ImGui::TextColored({0.45f, 0.6f, 1.0f, 1.0f}, "Read");
    ImGui::SameLine();
    ImGui::TextDisabled("(%zu bytes in each cell, 0x%zX bytes in each row)", block, block * heat_map_columns);

    const float available = ImGui::GetContentRegionAvail().x;
    const float cell_size = std::max(2.0f, std::floor(std::min(available / static_cast<float>(heat_map_columns), ImGui::GetFontSize())));
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    auto* draw_list = ImGui::GetWindowDrawList();

    for (size_t cell = 0; cell < cells; ++cell) {
        const float red   = intensity(block_sum(executions, cell), max_executions);
        const float green = intensity(block_sum(writes, cell), max_writes);
        const float blue  = intensity(block_sum(reads, cell), max_reads);

        const float x = origin.x + (static_cast<float>(cell % heat_map_columns) * cell_size);
        const float y = origin.y + (static_cast<float>(cell / heat_map_columns) * cell_size);
        const auto color = ImGui::ColorConvertFloat4ToU32({std::max(red, 0.08f), std::max(green, 0.08f), std::max(blue, 0.10f), 1.0f});
        draw_list->AddRectFilled({x, y}, {x + cell_size - 1.0f, y + cell_size - 1.0f}, color);
    }

    const ImVec2 size = {cell_size * static_cast<float>(heat_map_columns), cell_size * static_cast<float>(rows)};
    ImGui::InvisibleButton("##heat", size);

    if (!ImGui::IsItemHovered()) {
        return;
    }

    const ImVec2 mouse = ImGui::GetMousePos();
    const auto column = static_cast<size_t>((mouse.x - origin.x) / cell_size);
    const auto row = static_cast<size_t>((mouse.y - origin.y) / cell_size);
    const size_t cell = (row * heat_map_columns) + column;

    if ((column < heat_map_columns) && (cell < cells)) {
        const size_t first = cell * block;
        const auto name = address_name(static_cast<uint32_t>(first), context.document.program_labels());
        ImGui::SetTooltip("0x%04zX to 0x%04zX%s%s\nExecuted: %llu\nWritten: %llu\nRead: %llu",
            first, first + block - 1, name.empty() ? "" : "  ", name.c_str(),
            static_cast<unsigned long long>(block_sum(executions, cell)),
            static_cast<unsigned long long>(block_sum(writes, cell)),
            static_cast<unsigned long long>(block_sum(reads, cell)));
    }
}

} // namespace c8
