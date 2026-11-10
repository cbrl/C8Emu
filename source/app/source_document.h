#pragma once

#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

#include <imgui-color-text-edit/TextEditor.h>

#include "assembler/assembler.h"
#include "debugger/line_tracker.h"

namespace c8 {

/// The bytes in the loaded program that a line of the text wrote
struct LineBytes {
    uint32_t line    = 0; ///< The line that wrote the bytes. It can be after the line that was asked for.
    uint16_t address = 0;
    uint16_t size    = 0;
};


/**
 * @class SourceDocument
 *
 * @brief The assembly source in the code editor
 *
 * @details The document owns the editor widget and its file. It assembles the
 *          text shortly after an edit and shows the problems as markers and
 *          squiggles.
 *
 *          After a load, each line that wrote bytes has a tag with its address.
 *          The tags follow their lines through edits (see LineTracker), so the
 *          breakpoints and the PC show at the correct lines until the next load.
 *
 *          After each edit, the document compares the text with the saved
 *          text. So an undo back to the saved text makes the text clean again.
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

    /// Replace the text. The new text counts as saved in `source_file` (empty: no file). No program loaded from it.
    auto replace(std::string_view new_text, std::filesystem::path source_file) -> void;

    /// The text is now saved in a file
    auto mark_saved(std::filesystem::path source_file, std::string saved) -> void;

    /// The text changed since it was opened or saved
    [[nodiscard]]
    auto is_dirty() const noexcept -> bool {
        return dirty;
    }

    /// Assemble the text now and show the problems
    auto check() -> const AssembleResult&;

    /// The platform of a text without a `platform` line. A change checks the text again.
    auto set_default_platform(Variant variant) -> void;

    /// The result of the last check
    [[nodiscard]]
    auto last_check() const noexcept -> const AssembleResult& {
        return checked;
    }

    /// The result of the last check is now the program in the machine. The check must be of the current text.
    auto mark_loaded() -> void;

    /// The tags of the lines, from the program that last loaded from the text
    [[nodiscard]]
    auto lines() const noexcept -> const LineTracker& {
        return tracker;
    }

    /**
     * @brief The bytes in the loaded program that a line wrote
     *
     * @details A line that writes no bytes (a comment, a label, a blank line)
     *          gives the bytes of the next line that does.
     *
     * @return The bytes, or a message that tells why the line has none
     */
    [[nodiscard]]
    auto bytes_of_line(size_t line) const -> std::expected<LineBytes, std::string>;

    /// Scroll the editor to the line that wrote an address, if the line is out of view
    auto reveal(uint16_t address) -> void;

    /// Show the problems and the line of the PC as line markers. Call each frame, before the editor draws.
    auto update_markers(std::optional<uint16_t> pc) -> void;

private:
    // The editor calls this after each edit, undo, and redo
    auto text_changed() -> void;

    // Show the diagnostics of the last check as squiggles
    auto show_squiggles() -> void;

    TextEditor text_editor;
    std::filesystem::path file;

    // The text at the last open or save
    std::string saved_text;
    bool dirty = false;

    LineTracker tracker;

    AssembleResult checked;
    uint64_t check_count = 0;
    Variant default_platform = Variant::chip8;

    // The state that the markers show
    std::optional<uint32_t> marked_pc_line;
    uint64_t marked_check = 0;
    bool markers_valid = false;
};

} // namespace c8
