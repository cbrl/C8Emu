#include "syntax_reference_panel.h"

#include "assembler/forms.h"

#include <array>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace c8 {

namespace {

struct Entry {
    std::string_view syntax;
    std::string_view meaning;
};

// A line with only a number. It is not a directive, but it is a way to write data.
constexpr auto data_word = Entry{"nnnn", "A line with only a number writes it as a word"};

constexpr auto syntax_rules = std::array{
    Entry{"label: ...", "Define a label at the address of the line. A label can stand alone on its line."},
    Entry{"; text", "A comment, to the end of the line"},
    Entry{"2A  0x2A", "Hexadecimal numbers. A number without a prefix is hexadecimal."},
    Entry{"#42  #-1", "Decimal numbers. A negative number is stored as two's complement."},
    Entry{"%0010_1010", "Binary numbers. A '_' can separate the digits after a prefix."},
    Entry{"%..##..##", "A binary number for a sprite row: a '.' is 0 and a '#' is 1"},
    Entry{"x+1  (x + 1)", "An expression. It has no spaces, or it is in parentheses, because a space separates two operands."},
    Entry{"* / %  + -", "Multiply, divide, remainder, add, and subtract, with the precedence of C"},
    Entry{"<< >>  & ^ |  ~", "Shifts, bitwise AND, XOR, OR, and NOT"},
    Entry{"-x  #-10  -0x10", "Negative values. '-10' is an error, because it would be -0x10."},
    Entry{"v0 to vF, i", "The registers"},
    Entry{"long", "The keyword of the 4-byte XO-CHIP instruction 'mov i, long nnnn'"},
    Entry{"a, b  a b", "Commas and spaces both separate the operands"},
};

constexpr auto table_flags = ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_SizingFixedFit;


// One table cell. The last column wraps its text.
auto cell(std::string_view text, bool wrap = false) -> void {
    ImGui::TableNextColumn();

    if (wrap) {
        ImGui::TextWrapped("%.*s", static_cast<int>(text.size()), text.data());
    }
    else {
        ImGui::TextUnformatted(text.data(), text.data() + text.size());
    }
}

auto passes(const ImGuiTextFilter& filter, std::string_view a, std::string_view b) -> bool {
    return filter.PassFilter(a.data(), a.data() + a.size()) || filter.PassFilter(b.data(), b.data() + b.size());
}

// The short list of the platforms of a form, for the table
auto platform_text(const InstructionForm& form) -> std::string {
    if ((form.variants & on_all) == on_all) {
        return "All";
    }
    if (form.variants == on_super) {
        return "SUPER-CHIP, XO-CHIP";
    }
    return platform_list(form.variants);
}

// The directives, and the line with only a number
auto directive_entries() -> std::vector<Entry> {
    auto entries = std::vector<Entry>{};
    for (const auto& directive : directive_infos()) {
        entries.push_back(Entry{directive.syntax, directive.summary});
    }
    entries.push_back(data_word);
    return entries;
}

// Renders a table of the assembly syntax and descriptions
auto entry_table(const char* id, std::span<const Entry> entries, const ImGuiTextFilter& filter) -> void {
    if (!ImGui::BeginTable(id, 2, table_flags)) {
        return;
    }

    ImGui::TableSetupColumn("Syntax");
    ImGui::TableSetupColumn("Meaning", ImGuiTableColumnFlags_WidthStretch);

    for (const auto& entry : entries) {
        if (passes(filter, entry.syntax, entry.meaning)) {
            ImGui::TableNextRow();
            cell(entry.syntax);
            cell(entry.meaning, true);
        }
    }

    ImGui::EndTable();
}

} // namespace


auto SyntaxReferencePanel::draw(UiContext& /*context*/) -> void {
    if (!ImGui::Begin(title(), &open)) {
        ImGui::End();
        return;
    }

    filter.Draw("Filter", ImGui::GetFontSize() * 12.0f);
    ImGui::SetItemTooltip("Show only the rows that hold this text, for example \"vX\" or \"skip\"");

    ImGui::SameLine();
    ImGui::SetNextItemWidth(ImGui::GetFontSize() * 9.0f);
    const auto preview = platform ? std::string{to_string(*platform)} : std::string{"All platforms"};
    if (ImGui::BeginCombo("##platform", preview.c_str())) {
        if (ImGui::Selectable("All platforms", !platform)) {
            platform.reset();
        }
        for (const auto variant : all_variants) {
            const auto name = std::string{to_string(variant)};
            if (ImGui::Selectable(name.c_str(), platform == variant)) {
                platform = variant;
            }
        }
        ImGui::EndCombo();
    }
    ImGui::SetItemTooltip("Show only the instructions of one platform");

    ImGui::SeparatorText("Instructions");

    if (ImGui::BeginTable("##instructions", 4, table_flags)) {
        ImGui::TableSetupColumn("Syntax");
        ImGui::TableSetupColumn("Opcode");
        ImGui::TableSetupColumn("Platforms");
        ImGui::TableSetupColumn("Effect", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableHeadersRow();

        for (const auto& form : instruction_forms()) {
            if (platform && !form.available_on(*platform)) {
                continue;
            }

            const auto syntax = format_syntax(form);
            if (passes(filter, syntax, form.summary)) {
                ImGui::TableNextRow();
                cell(syntax);
                cell(form.pattern);
                cell(platform_text(form));
                cell(form.summary, true);
            }
        }

        ImGui::EndTable();
    }

    ImGui::SeparatorText("Directives");
    static const auto directives = directive_entries();
    entry_table("##directives", directives, filter);

    ImGui::SeparatorText("Labels, numbers, expressions, and comments");
    entry_table("##rules", syntax_rules, filter);

    ImGui::End();
}

} // namespace c8
