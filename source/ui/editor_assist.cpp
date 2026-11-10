#include "editor_assist.h"

#include "app/source_document.h"
#include "assembler/expression.h"
#include "assembler/forms.h"
#include "chip8/chip8.h"
#include "debugger/debugger.h"

#include <algorithm>
#include <expected>
#include <format>

#include <imgui.h>

namespace c8 {

namespace {

constexpr auto error_color   = ImVec4{1.0f, 0.45f, 0.45f, 1.0f};
constexpr auto warning_color = ImVec4{0.95f, 0.75f, 0.25f, 1.0f};

// The width of the help, in font sizes
constexpr float help_width = 28.0f;


// The byte offset of a column. A column counts code points.
auto byte_offset(std::string_view text, uint32_t column) -> size_t {
    uint32_t count = 0;
    for (size_t pos = 0; pos < text.size(); ++pos) {
        if (starts_code_point(text[pos])) {
            if (count == column) {
                return pos;
            }
            ++count;
        }
    }
    return text.size();
}

// A value with its decimal form, for example "0x2A (#42)" or "#-5"
auto value_text(int64_t value) -> std::string {
    return (value < 0) ? std::format("#{}", value) : std::format("0x{:X} (#{})", value, value);
}

// The bits of a value in groups of 4, for example "0010_1010"
auto binary_text(int64_t value, int bits) -> std::string {
    auto text = std::string{};
    for (int bit = bits - 1; bit >= 0; --bit) {
        text += (((value >> bit) & 1) != 0) ? '1' : '0';
        if ((bit > 0) && ((bit % 4) == 0)) {
            text += '_';
        }
    }
    return text;
}

// The bits of a byte as a sprite row, for example "..#.#.#."
auto sprite_text(int64_t value) -> std::string {
    auto text = std::string{};
    for (int bit = 7; bit >= 0; --bit) {
        text += (((value >> bit) & 1) != 0) ? '#' : '.';
    }
    return text;
}

auto show_text(std::string_view text) -> void {
    ImGui::TextUnformatted(text.data(), text.data() + text.size());
}

auto show_wrapped(std::string_view text) -> void {
    ImGui::TextWrapped("%.*s", static_cast<int>(text.size()), text.data());
}

auto show_wrapped(std::string_view text, const ImVec4& color) -> void {
    ImGui::PushStyleColor(ImGuiCol_Text, color);
    show_wrapped(text);
    ImGui::PopStyleColor();
}

auto show_disabled(std::string_view text) -> void {
    show_wrapped(text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
}


// Draws the sections of the help, with a line between two sections
class HelpWriter {
public:
    auto section() -> void {
        if (sections > 0) {
            ImGui::Separator();
        }
        ++sections;
    }

    [[nodiscard]]
    auto empty() const noexcept -> bool {
        return sections == 0;
    }

private:
    int sections = 0;
};


auto draw_problems(HelpWriter& help, const AssembleResult& check, uint32_t line, const Token& token) -> void {
    const uint32_t begin = token.column;
    const uint32_t end = token.column + token.length;
    bool first = true;

    for (const auto& diagnostic : check.diagnostics) {
        const bool overlaps = (diagnostic.line == line) && (diagnostic.length > 0)
                           && (diagnostic.column < end) && ((diagnostic.column + diagnostic.length) > begin);
        if (!overlaps) {
            continue;
        }

        if (first) {
            help.section();
            first = false;
        }
        show_wrapped(diagnostic.message, (diagnostic.severity == Severity::error) ? error_color : warning_color);
    }
}


auto draw_forms(HelpWriter& help, std::string_view mnemonic, Variant variant) -> void {
    for (const auto& form : instruction_forms()) {
        if (form.mnemonic != mnemonic) {
            continue;
        }

        help.section();

        const bool available = form.available_on(variant);
        const auto title = std::format("{}    {}", format_syntax(form), form.pattern);
        if (available) {
            show_text(title);
        }
        else {
            show_disabled(title);
        }

        show_wrapped(form.summary);

        if ((form.variants & on_all) != on_all) {
            const auto note = available
                ? std::format("Only on {}.", platform_list(form.variants))
                : std::format("Only on {}. The platform is {}.", platform_list(form.variants), to_string(variant));
            show_disabled(note);
        }
    }
}


auto draw_register(HelpWriter& help, UiContext& context, std::string_view word, WordKind kind) -> void {
    const bool paused = context.debugger.has_program() && !context.debugger.is_running();
    const auto& state = context.chip.state();

    if (kind == WordKind::i_register) {
        help.section();
        if (paused) {
            show_text(std::format("i = 0x{:04X}", state.i));
        }
        else {
            show_disabled("The register I. Pause the program to see its value.");
        }
        return;
    }

    // A larger register number is an error, which the problems show
    const auto index = parse_v_register(word).value_or(0x10);
    if (index > 0xF) {
        return;
    }

    help.section();
    if (paused) {
        const unsigned value = state.v[index];
        show_text(std::format("v{:X} = 0x{:02X} (#{})", index, value, value));
    }
    else {
        show_disabled(std::format("The register v{:X}. Pause the program to see its value.", index));
    }
}


auto draw_number(HelpWriter& help, std::string_view word) -> void {
    // A number with an error shows only its problem
    const auto value = parse_number(word);
    if (!value) {
        return;
    }

    help.section();
    const auto number = *value;

    if (number < 0) {
        show_text(std::format("#{}", number));
        if (number >= -128) {
            show_disabled(std::format("0x{:02X} as a byte", number & 0xFF));
        }
        if (number >= -32768) {
            show_disabled(std::format("0x{:04X} as a word", number & 0xFFFF));
        }
        return;
    }

    show_text(std::format("0x{:X} = #{}", number, number));

    const int bits = (number > 0xFFFF) ? 32 : ((number > 0xFF) ? 16 : 8);
    show_disabled(std::format("%{}", binary_text(number, bits)));
    if (bits == 8) {
        show_disabled(std::format("Sprite row: %{}", sprite_text(number)));
    }
}


auto draw_symbol(HelpWriter& help, const SourceDocument& document, std::string_view name) -> void {
    help.section();

    const auto* label = document.find_definition(name);
    if (label == nullptr) {
        show_disabled(std::format("'{}' is not defined", name));
        return;
    }

    const auto* kind = (label->kind == LabelKind::label) ? "Label" : "Constant";
    if (label->known) {
        show_text(std::format("{} {} = {}", kind, label->name, value_text(label->value)));
    }
    else {
        show_text(std::format("{} {} has no value, because its value has an error", kind, label->name));
    }
    show_disabled(std::format("Defined on line {}", label->line + 1));
}


auto draw_expression_value(HelpWriter& help, const SourceDocument& document, const Token& token) -> void {
    const auto expression = parse_expression(token.text, token.column);
    if (!expression) {
        return;
    }

    const auto lookup = [&document](std::string_view name) -> std::expected<int64_t, std::string> {
        const auto* label = document.find_definition(name);
        if ((label == nullptr) || !label->known) {
            return std::unexpected(std::string{});
        }
        return label->value;
    };

    if (const auto value = evaluate(*expression, lookup)) {
        help.section();
        show_text(std::format("{} = {}", token.text, value_text(*value)));
    }
}

} // namespace


auto source_word_at(std::string_view line, uint32_t column) -> std::optional<SourceWord> {
    const auto tokens = tokenize_line(line);

    const auto it = std::ranges::find_if(tokens, [column](const Token& token) {
        return (column >= token.column) && (column < (token.column + token.length));
    });
    if (it == tokens.end()) {
        return std::nullopt;
    }

    const auto index = static_cast<size_t>(it - tokens.begin());
    const size_t head_index = statement_start(tokens);

    auto word = SourceWord{
        .token  = *it,
        .text   = it->text,
        .column = it->column,
        .length = it->length,
        .head   = (index == head_index),
    };

    if (head_index < tokens.size()) {
        word.statement = to_lower_ascii(tokens[head_index].text);
    }

    word.kind = classify_word(it->text, word.head ? WordPosition::head : WordPosition::operand);

    if (word.kind == WordKind::label_definition) {
        word.text.remove_suffix(1);
        word.length -= 1;
        return word;
    }

    // An expression: find the part at the column
    if ((word.kind == WordKind::invalid) && !word.head) {
        const auto parts = split_expression(it->text, it->column);
        word.in_expression = (parts.size() > 1);

        const auto part = std::ranges::find_if(parts, [column](const ExpressionPart& p) {
            return (column >= p.column) && (column < (p.column + p.length));
        });
        if (part != parts.end()) {
            word.text = part->text;
            word.column = part->column;
            word.length = part->length;

            switch (part->kind) {
                case PartKind::number: word.kind = WordKind::number; break;
                case PartKind::name:   word.kind = classify_word(part->text, WordPosition::operand); break;
                default:               word.kind = WordKind::invalid; break;
            }
        }
    }

    return word;
}


auto draw_source_help(UiContext& context, uint32_t line, const SourceWord& word) -> bool {
    const auto& document = context.document;
    const auto& check = document.last_check();
    auto help = HelpWriter{};

    ImGui::PushTextWrapPos(ImGui::GetFontSize() * help_width);

    draw_problems(help, check, line, word.token);

    const auto lowercase = to_lower_ascii(word.text);

    // The operand of `platform` is a platform name, not a label
    if (!word.head && (word.statement == "platform")) {
        if (const auto variant = variant_from_id(lowercase)) {
            help.section();
            show_text(std::format("The platform {}", to_string(*variant)));
        }
        ImGui::PopTextWrapPos();
        return !help.empty();
    }

    switch (word.kind) {
        case WordKind::mnemonic: {
            draw_forms(help, lowercase, check.variant);
            break;
        }
        case WordKind::directive: {
            if (const auto* info = find_directive(lowercase)) {
                help.section();
                show_text(info->syntax);
                show_wrapped(info->summary);
            }
            break;
        }
        case WordKind::v_register:
        case WordKind::i_register: {
            draw_register(help, context, word.text, word.kind);
            break;
        }
        case WordKind::long_keyword: {
            help.section();
            show_text("long");
            show_wrapped("The keyword of 'mov i, long nnnn' (XO-CHIP). The address nnnn is a second word, so the instruction has 4 bytes.");
            break;
        }
        case WordKind::number: {
            draw_number(help, word.text);
            break;
        }
        case WordKind::label:
        case WordKind::label_definition: {
            draw_symbol(help, document, word.text);
            break;
        }
        case WordKind::invalid: {
            break;
        }
    }

    if (word.in_expression) {
        draw_expression_value(help, document, word.token);
    }

    ImGui::PopTextWrapPos();
    return !help.empty();
}


auto source_completions(std::string_view line, uint32_t term_start, uint32_t cursor, const AssembleResult& check)
    -> std::vector<std::string>
{
    const auto tokens = tokenize_line(line.substr(0, byte_offset(line, cursor)));
    if (tokens.empty()) {
        return {};
    }

    // The cursor must be at the end of a word
    const auto& current = tokens.back();
    if ((current.column + current.length) != cursor) {
        return {};
    }

    // The name before the cursor: the label characters at the end of the word
    size_t start = current.text.size();
    while ((start > 0) && is_label_char(current.text[start - 1])) {
        --start;
    }
    const auto typed = current.text.substr(start);
    if (typed.empty() || ((typed.front() >= '0') && (typed.front() <= '9'))) {
        return {};
    }

    // The editor replaces only the letters, digits, and '_' before the cursor. The characters before them stay.
    if ((term_start > cursor) || ((cursor - term_start) > typed.size())) {
        return {};
    }
    const size_t kept = typed.size() - (cursor - term_start);

    // A name inside an expression can only be a label or a constant
    const bool whole_word = (start == 0);

    const size_t index = tokens.size() - 1;
    const size_t head_index = statement_start(tokens);

    auto candidates = std::vector<std::string>{};
    bool symbols = false;

    if (index == head_index) {
        if (whole_word) {
            for (const auto& form : instruction_forms()) {
                if (form.available_on(check.variant)) {
                    candidates.emplace_back(form.mnemonic);
                }
            }
            for (const auto& directive : directive_infos()) {
                if (directive.name != "equ") {
                    candidates.emplace_back(directive.name);
                }
            }
            std::ranges::sort(candidates);
        }
    }
    else if (index > head_index) {
        const auto& head_token = tokens[head_index];
        const auto head = to_lower_ascii(head_token.text);
        const size_t operand = index - head_index - 1;
        const bool constant = (index > (head_index + 1)) && defines_constant(tokens, head_index);

        if (constant) {
            symbols = true;
        }
        else if ((operand == 0) && (classify_word(head_token.text, WordPosition::head) == WordKind::label)) {
            if (whole_word) {
                candidates.emplace_back("equ");
            }
        }
        else if (head == "platform") {
            if (whole_word && (operand == 0)) {
                for (const auto variant : all_variants) {
                    candidates.emplace_back(variant_id(variant));
                }
            }
        }
        else if (is_directive(head)) {
            symbols = true;
        }
        else if (is_mnemonic(head)) {
            // The forms that accept the operands before this one
            auto prior = std::vector<WordKind>{};
            for (size_t k = head_index + 1; k < index; ++k) {
                prior.push_back(classify_word(tokens[k].text, WordPosition::operand));
            }

            bool registers = false;
            bool v0_only = false;
            bool i_register = false;
            bool long_keyword = false;

            for (const auto& form : instruction_forms()) {
                if ((form.mnemonic != head) || !form.available_on(check.variant) || (form.operand_count <= operand)) {
                    continue;
                }

                const auto kinds = form.operands();
                bool match = true;
                for (size_t k = 0; k < prior.size(); ++k) {
                    match = match && operand_fits(kinds[k], prior[k]);
                }
                if (!match) {
                    continue;
                }

                switch (kinds[operand]) {
                    case OperandKind::vx:
                    case OperandKind::vy:      registers = true; break;
                    case OperandKind::v0:      v0_only = true; break;
                    case OperandKind::i:       i_register = true; break;
                    case OperandKind::long_kw: long_keyword = true; break;
                    default:                   symbols = true; break;
                }
            }

            if (whole_word) {
                if (registers) {
                    for (unsigned r = 0; r < 16; ++r) {
                        candidates.push_back(std::format("v{:X}", r));
                    }
                }
                else if (v0_only) {
                    candidates.emplace_back("v0");
                }
                if (i_register) {
                    candidates.emplace_back("i");
                }
                if (long_keyword) {
                    candidates.emplace_back("long");
                }
            }
        }
    }

    if (symbols) {
        auto names = std::vector<std::string>{};
        for (const auto& label : check.labels) {
            names.push_back(label.name);
        }
        std::ranges::sort(names);
        candidates.insert(candidates.end(), names.begin(), names.end());
    }

    // Keep the candidates that start with the typed name and are longer
    const auto typed_lower = to_lower_ascii(typed);
    auto completions = std::vector<std::string>{};

    for (const auto& candidate : candidates) {
        const auto lower = to_lower_ascii(candidate);
        if ((lower.size() <= typed_lower.size()) || !lower.starts_with(typed_lower)) {
            continue;
        }

        auto completion = candidate.substr(kept);
        if (std::ranges::find(completions, completion) == completions.end()) {
            completions.push_back(std::move(completion));
        }
    }

    return completions;
}


auto definition_at(const SourceDocument& document, std::string_view line, uint32_t column) -> const Label* {
    const auto word = source_word_at(line, column);
    if (!word || ((word->kind != WordKind::label) && (word->kind != WordKind::label_definition))) {
        return nullptr;
    }
    if (!word->head && (word->statement == "platform")) {
        return nullptr;
    }
    return document.find_definition(word->text);
}


auto definition_column(std::string_view line, const Label& label) -> uint32_t {
    for (const auto& token : tokenize_line(line)) {
        auto name = token.text;
        if (name.ends_with(':')) {
            name.remove_suffix(1);
        }
        if (to_lower_ascii(name) == label.name) {
            return token.column;
        }
    }
    return 0;
}

} // namespace c8
