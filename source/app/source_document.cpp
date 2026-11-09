#include "source_document.h"

#include "frontend/rom_files.h"
#include "ui/chip8_language.h"

#include <format>
#include <map>
#include <utility>

namespace c8 {

namespace {

// The editor waits this long after an edit before it checks the text
constexpr int check_delay_ms = 300;

// The squiggle group of the diagnostics. ClearSquiggles(type) removes only this group.
constexpr size_t diagnostic_squiggles = 1;

constexpr ImU32 error_color        = IM_COL32(255, 70, 70, 255);
constexpr ImU32 warning_color      = IM_COL32(230, 180, 40, 255);
constexpr ImU32 error_line_color   = IM_COL32(255, 60, 60, 40);
constexpr ImU32 warning_line_color = IM_COL32(230, 180, 40, 30);
constexpr ImU32 breakpoint_color   = IM_COL32(200, 40, 40, 255);

} // namespace


SourceDocument::SourceDocument() {
    text_editor.SetLanguage(chip8_language());
    text_editor.SetChangeCallback([this] { (void)check(); }, check_delay_ms);
    (void)check();
}


auto SourceDocument::display_name() const -> std::string {
    return file.empty() ? std::string{"Untitled"} : path_to_utf8(file.filename());
}


auto SourceDocument::replace(std::string_view new_text, std::filesystem::path source_file) -> void {
    // SetText also removes the markers, the squiggles, and the undo history
    text_editor.SetText(new_text);

    file = std::move(source_file);

    // The editor removes '\r', so compare with the text in the editor, not the input
    saved_text = text_editor.GetText();
    loaded_text.clear();
    loaded_map = {};
    has_loaded = false;
    markers_valid = false;

    (void)check();
}


auto SourceDocument::mark_saved(std::filesystem::path source_file, std::string saved) -> void {
    file = std::move(source_file);
    saved_text = std::move(saved);
    (void)take_snapshot();
}


auto SourceDocument::is_dirty() const noexcept -> bool {
    return (text_editor.GetUndoIndex() != snapshot_undo_index) || dirty_at_snapshot;
}


auto SourceDocument::check() -> const AssembleResult& {
    const auto text = take_snapshot();

    checked = assemble(text);
    ++check_count;

    show_squiggles();
    return checked;
}


auto SourceDocument::mark_loaded() -> void {
    loaded_text = text_editor.GetText();
    loaded_map = checked.map;
    has_loaded = true;
    (void)take_snapshot();
}


auto SourceDocument::differs_from_loaded() const noexcept -> bool {
    return !has_loaded || (text_editor.GetUndoIndex() != snapshot_undo_index) || differs_at_snapshot;
}


auto SourceDocument::address_of_line(size_t line) const -> std::optional<uint16_t> {
    if (differs_from_loaded()) {
        return std::nullopt;
    }

    const auto* span = loaded_map.span_for_line(static_cast<uint32_t>(line), true);
    if (span == nullptr) {
        return std::nullopt;
    }
    return span->address;
}


auto SourceDocument::update_markers(const std::set<uint16_t>& breakpoints) -> void {
    const bool differs = differs_from_loaded();

    if (markers_valid && (breakpoints == marked_breakpoints) && (check_count == marked_check) && (differs == marked_differs)) {
        return;
    }

    marked_breakpoints = breakpoints;
    marked_check = check_count;
    marked_differs = differs;
    markers_valid = true;

    struct LineMarks {
        bool has_problem = false;
        bool has_error = false;
        std::optional<uint16_t> breakpoint;
    };
    auto lines = std::map<uint32_t, LineMarks>{};

    for (const auto& diagnostic : checked.diagnostics) {
        auto& marks = lines[diagnostic.line];
        marks.has_problem = true;
        marks.has_error = marks.has_error || (diagnostic.severity == Severity::error);
    }

    // After an edit, the lines of the loaded program are not known. Show no breakpoints until the next load.
    if (!differs) {
        for (const auto address : breakpoints) {
            if (const auto* span = loaded_map.span_containing(address)) {
                auto& marks = lines[span->line];
                if (!marks.breakpoint) {
                    marks.breakpoint = address;
                }
            }
        }
    }

    // An editor line has one marker. The line number shows the breakpoint, and the line background shows the problem.
    text_editor.ClearMarkers();

    for (const auto& [line, marks] : lines) {
        const ImU32 number_color = marks.breakpoint ? breakpoint_color : 0;
        const ImU32 line_color = !marks.has_problem ? 0 : (marks.has_error ? error_line_color : warning_line_color);
        const auto tooltip = marks.breakpoint ? std::format("Breakpoint at 0x{:03X}", *marks.breakpoint) : std::string{};

        text_editor.AddMarker(line, number_color, line_color, tooltip, "");
    }
}


auto SourceDocument::take_snapshot() -> std::string {
    auto text = text_editor.GetText();

    snapshot_undo_index = text_editor.GetUndoIndex();
    dirty_at_snapshot = (text != saved_text);
    differs_at_snapshot = !has_loaded || (text != loaded_text);

    return text;
}


auto SourceDocument::show_squiggles() -> void {
    text_editor.ClearSquiggles(diagnostic_squiggles);

    for (const auto& diagnostic : checked.diagnostics) {
        const auto start = TextEditor::DocPos{diagnostic.line, diagnostic.column};
        const auto end = TextEditor::DocPos{diagnostic.line, diagnostic.column + diagnostic.length};
        const auto color = (diagnostic.severity == Severity::error) ? error_color : warning_color;

        text_editor.AddSquiggle(start, end, diagnostic_squiggles, color, diagnostic.message);
    }
}

} // namespace c8
