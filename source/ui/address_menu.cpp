#include "address_menu.h"

#include "app/source_document.h"
#include "chip8/chip8.h"
#include "debugger/address_names.h"
#include "debugger/debug_expression.h"
#include "debugger/debugger.h"
#include "util/log.h"

#include <format>

namespace c8 {

auto address_with_name(const UiContext& context, uint32_t address) -> std::string {
    const auto name = address_name(address, context.document.program_labels());
    return name.empty() ? std::format("0x{:04X}", address) : std::format("0x{:04X}  {}", address, name);
}


auto evaluate_address(const UiContext& context, std::string_view text) -> std::expected<uint16_t, std::string> {
    const auto& state = context.chip.state();

    const auto expression = DebugExpression::compile(text, context.debugger.symbols());
    if (!expression) {
        return std::unexpected(expression.error());
    }

    const auto value = expression->evaluate(state);
    if ((value < 0) || (static_cast<uint64_t>(value) >= state.memory.size())) {
        return std::unexpected(std::format("{} is not an address. Use 0 to 0x{:X}.", format_debug_value(value), state.memory.size() - 1));
    }
    return static_cast<uint16_t>(value);
}


auto draw_address_menu_items(UiContext& context, uint32_t address, std::optional<ShowTarget> here, uint16_t size) -> void {
    auto& debugger = context.debugger;
    auto& requests = context.requests;
    const auto target = static_cast<uint16_t>(address);

    // A log message can name a number that is not an address of this machine
    if (address >= context.chip.state().memory.size()) {
        ImGui::TextDisabled("0x%X is outside the memory of this platform", address);
        return;
    }

    // The items that show the address in another panel
    if (here != ShowTarget::code_editor) {
        const bool has_line = context.document.lines().line_of(target).has_value();
        if (ImGui::MenuItem("Show in Code Editor", nullptr, false, has_line)) {
            requests.show_in(ShowTarget::code_editor, address);
        }
        if (!has_line) {
            ImGui::SetItemTooltip("No line of the Code Editor wrote this address");
        }
    }
    if ((here != ShowTarget::program) && ImGui::MenuItem("Show in Program")) {
        requests.show_in(ShowTarget::program, address);
    }
    if ((here != ShowTarget::memory) && ImGui::MenuItem("Show in Memory")) {
        requests.show_in(ShowTarget::memory, address);
    }
    if ((here != ShowTarget::sprites) && ImGui::MenuItem("Show as Sprite")) {
        requests.show_in(ShowTarget::sprites, address);
    }

    ImGui::Separator();

    const bool has_breakpoint = debugger.has_breakpoint_in(target, size);
    if (ImGui::MenuItem(has_breakpoint ? "Remove Breakpoint" : "Add Breakpoint")) {
        (void)debugger.toggle_breakpoint_in(target, size);
    }

    if (ImGui::MenuItem("Run to Here", nullptr, false, debugger.has_program())) {
        debugger.run_to(target);
    }
    ImGui::SetItemTooltip("Run until the PC reaches this address");

    if (ImGui::MenuItem("Set PC Here", nullptr, false, debugger.has_program() && !debugger.is_running())) {
        context.chip.state().pc = target;

        // A replay from the history would not give the new PC
        requests.state_edited = true;
    }
    ImGui::SetItemTooltip("The next step or run starts at this address. Pause the program first.");

    if (ImGui::MenuItem("Watch Writes")) {
        (void)debugger.add_watchpoint(target, 1, WatchAccess::write);
        log_info("Added a watchpoint at 0x{:04X}. The program stops before an instruction writes the byte.", address);
    }
    ImGui::SetItemTooltip("Stop before an instruction writes this byte. The Breakpoints panel lists the watchpoint.");

    ImGui::Separator();

    if (ImGui::MenuItem("Copy Address")) {
        const auto text = std::format("0x{:04X}", address);
        ImGui::SetClipboardText(text.c_str());
    }
}


auto address_context_menu(UiContext& context, uint32_t address, std::optional<ShowTarget> here) -> void {
    if (!ImGui::BeginPopupContextItem("##address_menu")) {
        return;
    }

    const auto title = address_with_name(context, address);
    ImGui::TextDisabled("%s", title.c_str());
    ImGui::Separator();

    draw_address_menu_items(context, address, here);

    ImGui::EndPopup();
}

} // namespace c8
