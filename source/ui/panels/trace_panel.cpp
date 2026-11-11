#include "trace_panel.h"

#include "app/source_document.h"
#include "chip8/chip8.h"
#include "debugger/address_names.h"
#include "debugger/debugger.h"
#include "ui/address_menu.h"
#include "ui/dock_layout.h"

#include <algorithm>
#include <format>
#include <string>

namespace c8 {

namespace {

constexpr auto label_color = ImVec4{0.35f, 0.70f, 0.61f, 1.0f};

// A new filter checks at most this many entries in one frame, so the UI does not stop
constexpr uint64_t max_checks_per_frame = 20'000;


auto registers_text(const TraceEntry& entry) -> std::string {
    auto text = std::string{};
    for (size_t index = 0; index < entry.v.size(); ++index) {
        text += std::format("{:02X}{}", entry.v[index], ((index % 4) == 3) ? "  " : " ");
    }
    return text;
}

} // namespace


auto TracePanel::row_text(const UiContext& context, uint64_t sequence) const -> std::string {
    const auto& entry = context.debugger.trace().at(sequence);
    const auto instruction = format_instruction(entry.opcode, 0, context.chip.variant(), &names);
    const auto location = address_name(entry.pc, context.document.program_labels());

    return std::format("{} 0x{:04X} {} {:04X} {} I={:04X} SP={} {}",
        entry.cycle, entry.pc, location, entry.opcode, instruction, entry.i, entry.sp, registers_text(entry));
}


auto TracePanel::update_matches(const UiContext& context) -> void {
    const auto& trace = context.debugger.trace();
    const auto first = trace.first_sequence();
    const auto end = trace.end_sequence();
    const auto filter_text = std::string{filter.InputBuf};

    // A new filter text or a cleared trace checks all entries again
    if ((filter_text != checked_filter) || (end < checked_until)) {
        matches.clear();
        checked_until = first;
        checked_filter = filter_text;
    }

    // The ring buffer dropped the oldest entries
    while (!matches.empty() && (matches.front() < first)) {
        matches.pop_front();
    }
    checked_until = std::max(checked_until, first);

    const auto stop = std::min(end, checked_until + max_checks_per_frame);
    for (auto sequence = checked_until; sequence < stop; ++sequence) {
        if (filter.PassFilter(row_text(context, sequence).c_str())) {
            matches.push_back(sequence);
        }
    }
    checked_until = stop;
}


auto TracePanel::draw(UiContext& context) -> void {
    dock_with_on_first_use("Log");

    if (!ImGui::Begin(title(), &open)) {
        ImGui::End();
        return;
    }

    auto& debugger = context.debugger;
    const auto& trace = debugger.trace();
    const float font_size = ImGui::GetFontSize();

    // The names of the labels change with the program
    if (names_version != context.document.program_labels_version()) {
        names.clear();
        for (const auto& label : context.document.program_labels()) {
            names.try_emplace(static_cast<uint32_t>(label.value), label.name);
        }
        names_version = context.document.program_labels_version();
    }

    bool recording = trace.is_recording();
    if (ImGui::Checkbox("Record", &recording)) {
        debugger.set_tracing(recording);
    }
    ImGui::SetItemTooltip("Record each executed instruction. The trace keeps the newest %zu.", Trace::capacity);

    ImGui::SameLine();
    if (ImGui::Button("Clear")) {
        debugger.clear_trace();
        matches.clear();
        checked_until = 0;
    }

    ImGui::SameLine();
    ImGui::Checkbox("Follow", &follow);
    ImGui::SetItemTooltip("Scroll to the newest instruction while the program runs");

    ImGui::SameLine();
    const bool copy = ImGui::Button("Copy");
    ImGui::SetItemTooltip("Copy the rows that pass the filter");

    ImGui::SameLine();
    filter.Draw("Filter", font_size * 14.0f);
    ImGui::SetItemTooltip("Show only the rows that hold this text, for example an address, a label, or \"drw\"");

    const bool filtering = filter.IsActive();
    if (filtering) {
        update_matches(context);
    }
    else {
        checked_filter.clear();
    }

    const auto first = trace.first_sequence();
    const size_t row_count = filtering ? matches.size() : trace.size();
    const auto sequence_of = [&](size_t row) -> uint64_t {
        return filtering ? matches[row] : (first + row);
    };

    if (copy) {
        auto text = std::string{};
        for (size_t row = 0; row < row_count; ++row) {
            text += row_text(context, sequence_of(row));
            text += '\n';
        }
        ImGui::SetClipboardText(text.c_str());
    }

    if (filtering && (checked_until < trace.end_sequence())) {
        ImGui::TextDisabled("Checking the trace: %llu entries to go",
            static_cast<unsigned long long>(trace.end_sequence() - checked_until));
    }

    if (row_count == 0) {
        ImGui::TextDisabled("%s", recording ? "No instructions yet. Run or step the program."
                                            : "Turn on Record to trace the executed instructions.");
        ImGui::End();
        return;
    }

    constexpr auto table_flags = ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_Resizable;
    if (ImGui::BeginTable("##trace", 7, table_flags)) {
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableSetupColumn("Cycle", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn("Address", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn("Location", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn("Instruction", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("I", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn("SP", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn("v0-vF (before)", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableHeadersRow();

        const auto& labels = context.document.program_labels();
        const auto variant = context.chip.variant();

        auto clipper = ImGuiListClipper{};
        clipper.Begin(static_cast<int>(row_count));
        while (clipper.Step()) {
            for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row) {
                const auto& entry = trace.at(sequence_of(static_cast<size_t>(row)));

                ImGui::PushID(row);
                ImGui::TableNextRow();

                ImGui::TableNextColumn();
                ImGui::Text("%llu", static_cast<unsigned long long>(entry.cycle));

                ImGui::TableNextColumn();
                if (ImGui::Selectable(std::format("0x{:04X}", entry.pc).c_str(), false, ImGuiSelectableFlags_SpanAllColumns)) {
                    context.requests.show_in(ShowTarget::code_editor, entry.pc);
                }
                address_context_menu(context, entry.pc, std::nullopt);

                ImGui::TableNextColumn();
                ImGui::TextColored(label_color, "%s", address_name(entry.pc, labels).c_str());

                ImGui::TableNextColumn();
                const auto instruction = format_instruction(entry.opcode, 0, variant, &names);
                ImGui::Text("%04X  %s", entry.opcode, instruction.c_str());

                ImGui::TableNextColumn();
                ImGui::Text("%04X", entry.i);

                ImGui::TableNextColumn();
                ImGui::Text("%u", static_cast<unsigned>(entry.sp));

                ImGui::TableNextColumn();
                ImGui::TextUnformatted(registers_text(entry).c_str());

                ImGui::PopID();
            }
        }

        if (follow && debugger.is_running()) {
            ImGui::SetScrollHereY(1.0f);
        }

        ImGui::EndTable();
    }

    ImGui::End();
}

} // namespace c8
