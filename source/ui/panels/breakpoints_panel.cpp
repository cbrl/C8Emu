#include "breakpoints_panel.h"

#include "assembler/disassembler.h"
#include "chip8/chip8.h"
#include "debugger/debugger.h"
#include "ui/address_menu.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <format>

#include <imgui_stdlib.h>

namespace c8 {

namespace {

constexpr auto error_color = ImVec4{1.0f, 0.4f, 0.4f, 1.0f};
constexpr ImU32 stopped_row_color = IM_COL32(255, 200, 0, 50);

constexpr auto access_names = std::array{"Read", "Write", "Read or write"};

auto access_from_index(int index) -> WatchAccess {
    switch (index) {
        case 0:  return WatchAccess::read;
        case 2:  return WatchAccess::read_write;
        default: return WatchAccess::write;
    }
}

auto access_name(WatchAccess access) -> const char* {
    switch (access) {
        case WatchAccess::read:       return access_names[0];
        case WatchAccess::write:      return access_names[1];
        case WatchAccess::read_write: return access_names[2];
    }
    return access_names[1];
}

auto show_error(const std::string& error) -> void {
    if (!error.empty()) {
        ImGui::PushStyleColor(ImGuiCol_Text, error_color);
        ImGui::TextWrapped("%s", error.c_str());
        ImGui::PopStyleColor();
    }
}

} // namespace


auto BreakpointsPanel::parse_address(const UiContext& context, const std::string& text, std::string& error) -> std::optional<uint16_t> {
    const auto address = evaluate_address(context, text);
    if (!address) {
        error = address.error();
        return std::nullopt;
    }

    error.clear();
    return *address;
}


auto BreakpointsPanel::draw(UiContext& context) -> void {
    if (!ImGui::Begin(title(), &open)) {
        ImGui::End();
        return;
    }

    const auto& debugger = context.debugger;

    if (ImGui::BeginTabBar("##kinds")) {
        const auto breakpoints_label = std::format("Breakpoints ({})###breakpoints", debugger.breakpoints().size());
        if (ImGui::BeginTabItem(breakpoints_label.c_str())) {
            draw_breakpoints(context);
            ImGui::EndTabItem();
        }

        const auto watchpoints_label = std::format("Watchpoints ({})###watchpoints", debugger.watchpoints().size());
        if (ImGui::BeginTabItem(watchpoints_label.c_str())) {
            draw_watchpoints(context);
            ImGui::EndTabItem();
        }

        const auto* condition_label = debugger.break_condition().value ? "Break When (on)###condition" : "Break When###condition";
        if (ImGui::BeginTabItem(condition_label)) {
            draw_condition(context);
            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }

    ImGui::End();
}


auto BreakpointsPanel::draw_breakpoints(UiContext& context) -> void {
    auto& debugger = context.debugger;
    const auto& state = context.chip.state();

    // Add a breakpoint with Enter or with the button
    ImGui::SetNextItemWidth(ImGui::GetFontSize() * 10.0f);
    bool add = ImGui::InputTextWithHint("##address", "Address or label", &address_text, ImGuiInputTextFlags_EnterReturnsTrue);
    if (add) {
        // Keep the focus in the field for the next address
        ImGui::SetKeyboardFocusHere(-1);
    }
    ImGui::SetItemTooltip("A number, a label, or an expression, for example loop+4");

    ImGui::SameLine();
    add |= ImGui::Button("Add");
    if (add) {
        if (const auto address = parse_address(context, address_text, address_error)) {
            (void)debugger.add_breakpoint(*address);
            address_text.clear();
        }
    }

    ImGui::SameLine();
    ImGui::BeginDisabled(debugger.breakpoints().empty());
    if (ImGui::Button("Reset Hits")) {
        debugger.reset_hit_counts();
    }
    ImGui::SameLine();
    if (ImGui::Button("Remove All")) {
        debugger.clear_breakpoints();
    }
    ImGui::EndDisabled();

    show_error(address_error);

    if (debugger.breakpoints().empty()) {
        ImGui::TextDisabled("No breakpoints. Click the breakpoint column of the Code Editor or the Program panel, or enter an address.");
        return;
    }

    // Forget the text fields of removed breakpoints
    std::erase_if(edits, [&](const auto& entry) { return !debugger.has_breakpoint(entry.first); });

    constexpr auto table_flags = ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_ScrollY | ImGuiTableFlags_Resizable;
    if (!ImGui::BeginTable("##breakpoints", 8, table_flags)) {
        return;
    }

    ImGui::TableSetupScrollFreeze(0, 1);
    ImGui::TableSetupColumn("On", ImGuiTableColumnFlags_WidthFixed);
    ImGui::TableSetupColumn("Address", ImGuiTableColumnFlags_WidthFixed);
    ImGui::TableSetupColumn("Instruction", ImGuiTableColumnFlags_WidthStretch, 1.0f);
    ImGui::TableSetupColumn("Condition", ImGuiTableColumnFlags_WidthStretch, 1.0f);
    ImGui::TableSetupColumn("Hits", ImGuiTableColumnFlags_WidthFixed);
    ImGui::TableSetupColumn("Ignore", ImGuiTableColumnFlags_WidthFixed);
    ImGui::TableSetupColumn("Log Message", ImGuiTableColumnFlags_WidthStretch, 1.0f);
    ImGui::TableSetupColumn("##remove", ImGuiTableColumnFlags_WidthFixed);
    ImGui::TableHeadersRow();

    // Remove after the loop, so the loop does not change the map that it reads
    auto removed = std::optional<uint16_t>{};

    for (const auto address : debugger.breakpoint_addresses()) {
        auto& breakpoint = *debugger.find_breakpoint(address);

        auto [edit, added] = edits.try_emplace(address);
        if (added) {
            edit->second.condition = breakpoint.condition.text;
            edit->second.log = breakpoint.log.text;
        }
        auto& fields = edit->second;

        ImGui::PushID(address);
        ImGui::TableNextRow();

        // The machine stopped at this breakpoint
        if (!debugger.is_running() && (state.pc == address)) {
            ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg1, stopped_row_color);
        }

        ImGui::TableNextColumn();
        ImGui::Checkbox("##enabled", &breakpoint.enabled);
        ImGui::SetItemTooltip("A breakpoint that is off stays in the list, but it does not stop or log");

        ImGui::TableNextColumn();
        if (ImGui::Selectable(address_with_name(context, address).c_str())) {
            context.requests.show_in(ShowTarget::code_editor, address);
        }
        ImGui::SetItemTooltip("Show the line in the Code Editor. Right click for address menu.");
        address_context_menu(context, address, std::nullopt);

        ImGui::TableNextColumn();
        const auto text = format_instruction(state.read_word(address), state.read_word(address + 2u), context.chip.variant());
        ImGui::TextUnformatted(text.c_str());

        ImGui::TableNextColumn();
        ImGui::SetNextItemWidth(-FLT_MIN);
        ImGui::InputTextWithHint("##condition", "always", &fields.condition);
        if (ImGui::IsItemDeactivatedAfterEdit()) {
            (void)debugger.set_breakpoint_condition(address, fields.condition);
        }
        ImGui::SetItemTooltip("Stop only when this is true, e.g.: v0 == 5 && [i] != 0");
        show_error(breakpoint.condition.error);

        ImGui::TableNextColumn();
        ImGui::Text("%u", breakpoint.hit_count);

        ImGui::TableNextColumn();
        ImGui::SetNextItemWidth(ImGui::GetFontSize() * 4.0f);
        ImGui::InputScalar("##ignore", ImGuiDataType_U32, &breakpoint.ignore_count);
        ImGui::SetItemTooltip("The first hits that do not stop or log");

        ImGui::TableNextColumn();
        ImGui::SetNextItemWidth(-FLT_MIN);
        ImGui::InputTextWithHint("##log", "(stop)", &fields.log);
        if (ImGui::IsItemDeactivatedAfterEdit()) {
            (void)debugger.set_breakpoint_log(address, fields.log);
        }
        ImGui::SetItemTooltip("With a message, the breakpoint writes to the Log and does not stop.\n"
                              "{v0} gives the value in hex, {v1:d} in decimal. Example: x = {v0:d}, sprite = {i}");
        show_error(breakpoint.log.error);

        ImGui::TableNextColumn();
        if (ImGui::SmallButton("X")) {
            removed = address;
        }
        ImGui::SetItemTooltip("Remove the breakpoint");

        ImGui::PopID();
    }

    ImGui::EndTable();

    if (removed) {
        debugger.remove_breakpoint(*removed);
    }
}


auto BreakpointsPanel::draw_watchpoints(UiContext& context) -> void {
    auto& debugger = context.debugger;
    const float font_size = ImGui::GetFontSize();

    ImGui::SetNextItemWidth(font_size * 9.0f);
    ImGui::InputTextWithHint("##watch_address", "Address or label", &watch_address);
    ImGui::SetItemTooltip("A number, a label, or an expression, for example score+1");

    ImGui::SameLine();
    ImGui::SetNextItemWidth(font_size * 5.0f);
    ImGui::InputInt("Bytes", &watch_length);
    watch_length = std::clamp(watch_length, 1, 0x1000);

    ImGui::SameLine();
    ImGui::SetNextItemWidth(font_size * 8.0f);
    ImGui::Combo("##access", &watch_access, access_names.data(), static_cast<int>(access_names.size()));

    ImGui::SameLine();
    if (ImGui::Button("Add##watch")) {
        if (const auto address = parse_address(context, watch_address, watch_error)) {
            (void)debugger.add_watchpoint(*address, static_cast<uint16_t>(watch_length), access_from_index(watch_access));
            watch_address.clear();
        }
    }

    ImGui::SameLine();
    ImGui::BeginDisabled(debugger.watchpoints().empty());
    if (ImGui::Button("Remove All##watch")) {
        debugger.clear_watchpoints();
    }
    ImGui::EndDisabled();

    show_error(watch_error);

    if (debugger.watchpoints().empty()) {
        ImGui::TextDisabled("No watchpoints. A watchpoint stops the program before an instruction reads or writes its memory.");
        return;
    }

    constexpr auto table_flags = ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_ScrollY;
    if (!ImGui::BeginTable("##watchpoints", 5, table_flags)) {
        return;
    }

    ImGui::TableSetupScrollFreeze(0, 1);
    ImGui::TableSetupColumn("On", ImGuiTableColumnFlags_WidthFixed);
    ImGui::TableSetupColumn("Memory", ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableSetupColumn("Access", ImGuiTableColumnFlags_WidthFixed);
    ImGui::TableSetupColumn("Hits", ImGuiTableColumnFlags_WidthFixed);
    ImGui::TableSetupColumn("##remove", ImGuiTableColumnFlags_WidthFixed);
    ImGui::TableHeadersRow();

    auto removed = std::optional<uint32_t>{};

    for (const auto& watch : debugger.watchpoints()) {
        auto& live = *debugger.find_watchpoint(watch.id);

        ImGui::PushID(static_cast<int>(watch.id));
        ImGui::TableNextRow();

        ImGui::TableNextColumn();
        ImGui::Checkbox("##enabled", &live.enabled);

        ImGui::TableNextColumn();
        auto range = address_with_name(context, watch.address);
        if (watch.length > 1) {
            range += std::format("  to 0x{:04X}", watch.address + watch.length - 1);
        }
        if (ImGui::Selectable(range.c_str())) {
            context.requests.show_in(ShowTarget::memory, watch.address);
        }
        ImGui::SetItemTooltip("Show the memory. Right click for address menu.");
        address_context_menu(context, watch.address, std::nullopt);

        ImGui::TableNextColumn();
        ImGui::TextUnformatted(access_name(watch.access));

        ImGui::TableNextColumn();
        ImGui::Text("%u", watch.hit_count);

        ImGui::TableNextColumn();
        if (ImGui::SmallButton("X")) {
            removed = watch.id;
        }
        ImGui::SetItemTooltip("Remove the watchpoint");

        ImGui::PopID();
    }

    ImGui::EndTable();

    if (removed) {
        debugger.remove_watchpoint(*removed);
    }
}


auto BreakpointsPanel::draw_condition(UiContext& context) -> void {
    auto& debugger = context.debugger;
    const auto& condition = debugger.break_condition();

    if (!condition_loaded) {
        condition_text = condition.text;
        condition_loaded = true;
    }

    ImGui::TextWrapped("Stop before any instruction while this expression is true. "
                       "The check runs before each instruction, so the emulation is a little slower.");

    ImGui::SetNextItemWidth(-ImGui::GetFontSize() * 9.0f);
    const bool apply = ImGui::InputTextWithHint("##condition", "for example: i >= 0xE00 || sp > 10", &condition_text,
                                                ImGuiInputTextFlags_EnterReturnsTrue);
    ImGui::SameLine();
    if (ImGui::Button("Apply") || apply) {
        (void)debugger.set_break_condition(condition_text);
    }
    ImGui::SameLine();
    if (ImGui::Button("Clear")) {
        condition_text.clear();
        (void)debugger.set_break_condition("");
    }

    if (!condition.error.empty()) {
        show_error(condition.error);
    }
    else if (condition.value) {
        ImGui::TextDisabled("On: %s", condition.text.c_str());
    }
    else {
        ImGui::TextDisabled("Off");
    }

    ImGui::SeparatorText("Expressions");
    ImGui::BulletText("Registers: v0 to vF, i, pc, sp, dt, st. Counters: frame, cycles.");
    ImGui::BulletText("Memory: [address] reads a byte. w[address] reads a word.");
    ImGui::BulletText("Numbers as in the assembler: 2A and 0x2A are hex, #42 is decimal, %%0101 is binary.");
    ImGui::BulletText("The labels and the constants of the loaded program.");
    ImGui::BulletText("Operators: || && | ^ & == != < <= > >= << >> + - * / %% ! ~");
}

} // namespace c8
