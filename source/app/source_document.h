#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <set>
#include <string>
#include <string_view>

#include <imgui-color-text-edit/TextEditor.h>

#include "assembler/assembler.h"

namespace c8 {

/**
 * @class SourceDocument
 *
 * @brief The assembly source in the code editor
 *
 * @details The document owns the editor widget and its file. It assembles the
 *          text shortly after an edit and shows the problems as markers and
 *          squiggles. It also keeps the source map of the last program that
 *          loaded from the text, so a line can give an address.
 *
 *          The dirty state comes from the undo index of the editor, so it
 *          shows at once. The document compares the text after each check,
 *          so an undo back to the saved text makes it clean again.
 *
 *          The callbacks of the editor point to this object, so it cannot move.
 */
class SourceDocument {
public:
    SourceDocument();
    SourceDocument(const SourceDocument&) = delete;
    SourceDocument(SourceDocument&&) = delete;

	~SourceDocument() = default;

    auto operator=(const SourceDocument&) -> SourceDocument& = delete;
    auto operator=(SourceDocument&&) -> SourceDocument& = delete;

    [[nodiscard]]
    auto editor() noexcept -> TextEditor& {
        return text_editor;
    }

    [[nodiscard]]
    auto text() const -> std::string {
        return text_editor.GetText();
    }

    /// The file of the text. Empty if the text has no file (a new text or a disassembly).
    [[nodiscard]]
    auto path() const noexcept -> const std::filesystem::path& {
        return file;
    }

    /// The file name, or "Untitled"
    [[nodiscard]]
    auto display_name() const -> std::string;

    /// Replace the text. The new text counts as saved in `source_file` (empty: no file).
    auto replace(std::string_view new_text, std::filesystem::path source_file) -> void;

    /// The text is now saved in a file
    auto mark_saved(std::filesystem::path source_file, std::string saved) -> void;

    /// The text changed since it was opened or saved
    [[nodiscard]]
    auto is_dirty() const noexcept -> bool;

    /// Assemble the text now and show the problems
    auto check() -> const AssembleResult&;

    /// The result of the last check
    [[nodiscard]]
    auto last_check() const noexcept -> const AssembleResult& {
        return checked;
    }

    /// The result of the last check is now the program in the machine
    auto mark_loaded() -> void;

    /// The text changed since it was loaded, so the addresses of its lines are not known
    [[nodiscard]]
    auto differs_from_loaded() const noexcept -> bool;

    /**
     * @brief The program address of a line
     *
     * @details A line that writes no bytes gives the address of the next line
     *          that does.
     *
     * @return The address, or std::nullopt if the text differs from the
     *         loaded program or no code follows the line
     */
    [[nodiscard]]
    auto address_of_line(size_t line) const -> std::optional<uint16_t>;

    /// Show the problems and the breakpoints as line markers. Call each frame, before the editor draws.
    auto update_markers(const std::set<uint16_t>& breakpoints) -> void;

private:
    // Read the text and update the cached dirty and loaded states
    auto take_snapshot() -> std::string;

    // Show the diagnostics of the last check as squiggles
    auto show_squiggles() -> void;

    TextEditor text_editor;
    std::filesystem::path file;

    // The text at the last open or save, and at the last load into the machine
    std::string saved_text;
    std::string loaded_text;
    SourceMap loaded_map;
    bool has_loaded = false;

    AssembleResult checked;
    uint64_t check_count = 0;

    // The state at the last snapshot. A different undo index means that the text changed since then.
    size_t snapshot_undo_index = 0;
    bool dirty_at_snapshot = false;
    bool differs_at_snapshot = true;

    // The state that the markers show
    std::set<uint16_t> marked_breakpoints;
    uint64_t marked_check = 0;
    bool marked_differs = true;
    bool markers_valid = false;
};

} // namespace c8
