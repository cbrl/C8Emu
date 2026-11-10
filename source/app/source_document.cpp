#include "source_document.h"

#include "frontend/rom_files.h"
#include "ui/chip8_language.h"

#include <format>
#include <map>
#include <utility>
#include <vector>

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
constexpr ImU32 pc_line_color      = IM_COL32(255, 200, 0, 60);

} // namespace


SourceDocument::SourceDocument() {
    text_editor.SetLanguage(chip8_language());
    text_editor.SetChangeCallback([this] { (void)check(); }, check_delay_ms);
    text_editor.SetTransactionCallback([this](const std::vector<TextEditor::Change>&) { text_changed(); });
    (void)check();
}


auto SourceDocument::display_name() const -> std::string {
    return file.empty() ? std::string{"Untitled"} : path_to_utf8(file.filename());
}


auto SourceDocument::replace(std::string_view new_text, std::filesystem::path source_file) -> void {
    // SetText also removes the markers, the squiggles, and the undo history. It calls no callback.
    text_editor.SetText(new_text);

    file = std::move(source_file);

    // The editor removes '\r', so compare with the text in the editor, not the input
    saved_text = text_editor.GetText();
    dirty = false;

    tracker.clear();
    markers_valid = false;

    (void)check();
}


auto SourceDocument::mark_saved(std::filesystem::path source_file, std::string saved) -> void {
    file = std::move(source_file);
    saved_text = std::move(saved);
    dirty = (text_editor.GetText() != saved_text);
}


auto SourceDocument::check() -> const AssembleResult& {
    checked = assemble(text_editor.GetText(), {.variant = default_platform});
    ++check_count;

    show_squiggles();
    return checked;
}


auto SourceDocument::set_default_platform(Variant variant) -> void {
    if (variant != default_platform) {
        default_platform = variant;
        (void)check();
    }
}


auto SourceDocument::mark_loaded() -> void {
    tracker.load(text_editor.GetText(), checked.map);
    markers_valid = false;
}


auto SourceDocument::bytes_of_line(size_t line) const -> std::expected<LineBytes, std::string> {
    if (!tracker.has_program()) {
        return std::unexpected("No program loaded from this text. Assemble & Load (Ctrl+B) first.");
    }

    const auto target = tracker.statement_line_from(static_cast<uint32_t>(line));
    if (!target) {
        return std::unexpected("No instruction or data follows this line.");
    }

    const auto* span = tracker.span_at(*target);
    if (span == nullptr) {
        return std::unexpected(std::format(
            "Line {} is new since the last load, so it has no address. Assemble & Load (Ctrl+B) first.", *target + 1));
    }

    return LineBytes{.line = *target, .address = span->address, .size = span->size};
}


auto SourceDocument::reveal(uint16_t address) -> void {
    const auto line = tracker.line_of(address);
    if (!line) {
        return;
    }

    // The editor has no folds and no word wrap, so each row is one line. The last row can be cut off.
    const bool visible = (*line >= text_editor.GetFirstVisibleRow()) && (*line < text_editor.GetLastVisibleRow());
    if (!visible) {
        text_editor.ScrollToLine(*line, TextEditor::Scroll::alignMiddle);
    }
}


auto SourceDocument::update_markers(std::optional<uint16_t> pc) -> void {
    const auto pc_line = pc ? tracker.line_of(*pc) : std::nullopt;

    if (markers_valid && (check_count == marked_check) && (pc_line == marked_pc_line)) {
        return;
    }

    marked_check = check_count;
    marked_pc_line = pc_line;
    markers_valid = true;

    // true if the line has an error, false if it has only warnings
    auto problem_lines = std::map<uint32_t, bool>{};
    for (const auto& diagnostic : checked.diagnostics) {
        auto& has_error = problem_lines[diagnostic.line];
        has_error = has_error || (diagnostic.severity == Severity::error);
    }

    // An editor line has one marker. The PC line shows the PC, also if the line has a problem.
    text_editor.ClearMarkers();

    for (const auto& [line, has_error] : problem_lines) {
        if (line != pc_line) {
            text_editor.AddMarker(line, 0, has_error ? error_line_color : warning_line_color, "", "");
        }
    }

    if (pc_line) {
        text_editor.AddMarker(*pc_line, 0, pc_line_color, "", "");
    }
}


auto SourceDocument::text_changed() -> void {
    const auto text = text_editor.GetText();

    dirty = (text != saved_text);
    tracker.update(text);
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
