#include "editor_panel.h"

#include "app/session.h"
#include "chip8/chip8.h"
#include "debugger/debugger.h"
#include "instruction/instruction.h"
#include "util/log.h"

#include <string>
#include <string_view>
#include <vector>

namespace c8 {

EditorPanel::EditorPanel() {
    // HLSL highlighting is a placeholder until the assembler has its own language definition
    editor.SetLanguage(TextEditor::Language::Hlsl());
}


auto EditorPanel::draw(UiContext& context) -> void {
    if (context.session.rom_generation() != shown_rom) {
        shown_rom = context.session.rom_generation();
        show_disassembly(context);
    }

    update_breakpoint_markers(context);

    const float font_size = ImGui::GetFontSize();
    ImGui::SetNextWindowSize({font_size * 30.0f, font_size * 30.0f}, ImGuiCond_FirstUseEver);

    if (ImGui::Begin(title(), &open, ImGuiWindowFlags_MenuBar)) {
        if (ImGui::BeginMenuBar()) {
            if (ImGui::MenuItem("Compile")) {
                compile(context);
            }

            if (ImGui::BeginMenu("Breakpoint")) {
                // Each editor line holds one 2-byte instruction, starting at the program origin
                const auto cursor_line = editor.GetMainCursorPosition().line;
                const auto address = static_cast<uint16_t>(context.chip.program_origin() + (cursor_line * 2));

                if (ImGui::MenuItem("Add")) {
                    context.debugger.add_breakpoint(address);
                }
                if (ImGui::MenuItem("Remove")) {
                    context.debugger.remove_breakpoint(address);
                }
                if (ImGui::MenuItem("Clear")) {
                    context.debugger.clear_breakpoints();
                }

                ImGui::EndMenu();
            }

            ImGui::EndMenuBar();
        }

        editor.Render("editor");
    }
    ImGui::End();
}


auto EditorPanel::show_disassembly(const UiContext& context) -> void {
    const auto result = decompile_program(context.chip.program());

    for (const auto idx : result.failures) {
        log_warning("Disassembly: the word at offset 0x{:03X} (instruction {}) is not valid", idx * 2, idx);
    }

    // SetText removes all markers, so show the breakpoints again
    editor.SetText(std::vector<std::string_view>(result.program.begin(), result.program.end()));
    markers_valid = false;
}


auto EditorPanel::compile(UiContext& context) -> void {
    auto lines = std::vector<std::string>{};
    lines.reserve(editor.GetLineCount());
    for (size_t line = 0; line < editor.GetLineCount(); ++line) {
        lines.push_back(editor.GetLineText(line));
    }

    const auto result = compile_program(lines);

    for (const auto idx : result.failures) {
        log_warning("Compile: line {} is not a valid instruction", idx + 1);
    }

    // Store the words big-endian
    auto bytes = std::vector<uint8_t>{};
    bytes.reserve(result.program_data.size() * 2);
    for (const auto word : result.program_data) {
        bytes.push_back(static_cast<uint8_t>(word >> 8));
        bytes.push_back(static_cast<uint8_t>(word));
    }

    (void)context.session.load_assembled(bytes);
}


auto EditorPanel::update_breakpoint_markers(const UiContext& context) -> void {
    static constexpr ImU32 breakpoint_color = IM_COL32(255, 64, 64, 96);

    const auto& breakpoints = context.debugger.breakpoints();
    const auto origin = context.chip.program_origin();

    if (markers_valid && (breakpoints == marked_breakpoints) && (origin == marked_origin)) {
        return;
    }

    marked_breakpoints = breakpoints;
    marked_origin = origin;
    markers_valid = true;

    editor.ClearMarkers();

    // Each editor line holds one 2-byte instruction, so only even offsets from the origin have a line
    for (const auto address : breakpoints) {
        if ((address < origin) || (((address - origin) % 2) != 0)) {
            continue;
        }

        const auto line = static_cast<size_t>((address - origin) / 2);
        if (line < editor.GetLineCount()) {
            editor.AddMarker(line, breakpoint_color, breakpoint_color, "Breakpoint", "");
        }
    }
}

} // namespace c8
