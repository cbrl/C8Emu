#include "chip8_language.h"

#include "assembler/expression.h"
#include "assembler/lexer.h"

#include <algorithm>
#include <array>
#include <optional>
#include <string>
#include <string_view>

namespace c8 {

namespace {

using Color = TextEditor::Color;
using Iterator = TextEditor::Iterator;

// The separators of tokenize_line. A ';' starts a comment, which the editor colors.
auto is_separator_glyph(ImWchar c) -> bool {
    return (c < 0x80) && is_separator(static_cast<char>(c));
}

auto advance(Iterator it, size_t count) -> Iterator {
    for (size_t i = 0; i < count; ++i) {
        ++it;
    }
    return it;
}


// One colored piece of a word
struct Piece {
    size_t offset = 0; // In glyphs from the start of the word
    size_t length = 0; // In glyphs
    Color  color  = Color::text;
};

/**
 * The editor colors a line in one pass from left to right. It calls the
 * highlighter at the start of each piece of text that is not a space or a
 * comment. The highlighter reads a whole word at the first call, and it keeps
 * the pieces of the word for the next calls. It also counts the words of the
 * line, so it knows which word is the instruction or the directive.
 */
struct Highlighter {
    // The pass over the current line
    std::optional<Iterator> line_end;  // The end of the line
    size_t                  remaining = 0; // The glyphs from the last position to the end of the line

    std::optional<Iterator> word_start; // The start of the last word of the pass
    std::vector<Piece>      pieces;     // The pieces of the last word
    size_t                  next  = 0;  // The next piece to give
    size_t                  words = 0;  // The words of the statement before the last word. A label definition does not count.
    bool                    first = true; // The pass has read no word yet

    std::vector<std::string> constants; // Sorted

    auto start_pass(Iterator end, size_t glyphs) -> void {
        line_end = end;
        remaining = glyphs;
        word_start.reset();
        pieces.clear();
        next = 0;
        words = 0;
        first = true;
    }
};

auto highlighter() -> Highlighter& {
    static auto instance = Highlighter{};
    return instance;
}


/**
 * The editor calls this hook at the start of each piece of a line, also at a
 * space, from the first glyph of the line. In one pass, each call has fewer
 * glyphs to the end of the line than the call before. So more glyphs, or
 * another line, start a new pass. The hook finds no comment, so it returns
 * `start`.
 */
auto track_position(Iterator start, Iterator end, size_t& /*level*/) -> Iterator {
    auto& state = highlighter();
    const size_t remaining = end - start;

    if (!state.line_end || (*state.line_end != end) || (remaining >= state.remaining)) {
        state.start_pass(end, remaining);
    }
    else {
        state.remaining = remaining;
    }
    return start;
}


// The color of a name that is not a reserved word
auto name_color(std::string_view name) -> Color {
    const auto& constants = highlighter().constants;
    return std::ranges::binary_search(constants, to_lower_ascii(name)) ? Color::string : Color::identifier;
}

// The color of a word of a known kind. std::nullopt for an expression, which has pieces of different colors.
auto word_color(WordKind kind, std::string_view word) -> std::optional<Color> {
    switch (kind) {
        case WordKind::mnemonic:         return Color::keyword;
        case WordKind::directive:        return Color::preprocessor;
        case WordKind::v_register:
        case WordKind::i_register:       return Color::knownIdentifier;
        case WordKind::long_keyword:     return Color::keyword;
        case WordKind::number:           return Color::number;
        case WordKind::label:            return name_color(word);
        case WordKind::label_definition: return Color::declaration;
        case WordKind::invalid:          return std::nullopt;
    }
    return std::nullopt;
}

// The pieces of an operand that is an expression, for example "x+1"
auto expression_pieces(std::string_view text) -> std::vector<Piece> {
    auto pieces = std::vector<Piece>{};

    for (const auto& part : split_expression(text)) {
        auto color = Color::text;

        switch (part.kind) {
            case PartKind::number: {
                color = Color::number;
                break;
            }
            case PartKind::name: {
                color = word_color(classify_word(part.text, WordPosition::operand), part.text).value_or(Color::text);
                break;
            }
            case PartKind::op:
            case PartKind::open:
            case PartKind::close: {
                color = Color::punctuation;
                break;
            }
            case PartKind::invalid: {
                break;
            }
        }

        pieces.push_back(Piece{.offset = part.column, .length = part.length, .color = color});
    }

    return pieces;
}


// The text of the word at `start`, as tokenize_line reads it. `glyphs` gets its length.
auto read_word(Iterator start, Iterator end, size_t& glyphs) -> std::string {
    auto text = std::string{};
    int depth = 0;

    glyphs = 0;
    for (auto it = start; it != end; ++it) {
        const ImWchar c = *it;
        if ((c == ';') || ((depth == 0) && is_separator_glyph(c))) {
            break;
        }

        if (c == '(') {
            ++depth;
        }
        else if ((c == ')') && (depth > 0)) {
            --depth;
        }

        auto utf8 = std::array<char, 4>{};
        text.append(utf8.data(), TextEditor::CodePoint::write(utf8.data(), c));
        ++glyphs;
    }

    // A '(' without a ')' takes the rest of the line. The separators at the end are not part of the word.
    if (depth > 0) {
        while (!text.empty() && is_separator(text.back())) {
            text.pop_back();
            --glyphs;
        }
    }

    return text;
}

// True if the word after `it` is `equ`. Then the word before it is the name of a constant.
auto next_word_is_equ(Iterator it, Iterator end) -> bool {
    while ((it != end) && is_separator_glyph(*it)) {
        ++it;
    }

    auto word = std::string{};
    while ((it != end) && !is_separator_glyph(*it) && (*it != ';')) {
        if ((*it >= 0x80) || (word.size() == 3)) {
            return false;
        }
        word.push_back(static_cast<char>(*it));
        ++it;
    }

    return to_lower_ascii(word) == "equ";
}


// The pieces of a word that is not the label definition at the start of the line
auto word_pieces(std::string_view text, size_t glyphs, bool head, Iterator word_end, Iterator line_end) -> std::vector<Piece> {
    const auto kind = classify_word(text, head ? WordPosition::head : WordPosition::operand);

    if (const auto whole = word_color(kind, text)) {
        auto piece_color = *whole;

        // The name in `name equ value` is a constant
        if (head && (kind == WordKind::label) && next_word_is_equ(word_end, line_end)) {
            piece_color = Color::string;
        }
        return {Piece{.offset = 0, .length = glyphs, .color = piece_color}};
    }

    // A first word that is not an instruction, a directive, a number, or a name
    if (head) {
        return {Piece{.offset = 0, .length = glyphs, .color = Color::text}};
    }

    return expression_pieces(text);
}


auto tokenize(Iterator start, Iterator end, Color& color) -> Iterator {
    auto& state = highlighter();

    // The next piece of the last word
    if (state.word_start && (*state.word_start < start)) {
        const size_t offset = start - *state.word_start;

        while ((state.next < state.pieces.size()) && (state.pieces[state.next].offset < offset)) {
            ++state.next;
        }
        if ((state.next < state.pieces.size()) && (state.pieces[state.next].offset == offset)) {
            const auto& piece = state.pieces[state.next];
            ++state.next;
            color = piece.color;
            return advance(start, piece.length);
        }
    }

    // A new word
    state.word_start = start;
    state.pieces.clear();
    state.next = 0;

    size_t glyphs = 0;
    const auto text = read_word(start, end, glyphs);

    // A ',' between two words. The editor colors it as punctuation.
    if (glyphs == 0) {
        return start;
    }

    const bool first_word = state.first;
    state.first = false;

    // The assembler reads a first word that ends with ':' as a label definition, also if the name is not valid
    if (first_word && text.ends_with(':')) {
        const bool valid = (classify_word(text) == WordKind::label_definition);
        state.pieces.push_back(Piece{.offset = 0, .length = glyphs, .color = valid ? Color::declaration : Color::text});
    }
    else {
        const bool head = (state.words == 0);
        ++state.words;
        state.pieces = word_pieces(text, glyphs, head, advance(start, glyphs), end);
    }

    // The first piece always starts at the start of the word, because the word starts with a character that is not a space
    const auto& piece = state.pieces.front();
    state.next = 1;
    color = piece.color;
    return advance(start, piece.length);
}


auto make_language() -> TextEditor::Language {
    auto language = TextEditor::Language{};

    language.name = "CHIP-8";
    language.caseSensitive = false;
    language.singleLineComment = ";";
    language.isPunctuation = [](ImWchar c) { return (c == ',') || (c == ':'); };
    language.commentLevelStart = track_position;
    language.customTokenizer = tokenize;

    return language;
}

// Two objects with the same rules. A different object makes the editor color all lines again.
auto languages() -> std::array<TextEditor::Language, 2>& {
    static auto list = std::array{make_language(), make_language()};
    return list;
}

} // namespace


auto chip8_language(bool alternate) -> const TextEditor::Language* {
    return &languages()[alternate ? 1 : 0];
}


auto set_constant_names(std::vector<std::string> names) -> bool {
    std::ranges::sort(names);

    auto& constants = highlighter().constants;
    if (names == constants) {
        return false;
    }

    constants = std::move(names);
    return true;
}

} // namespace c8
