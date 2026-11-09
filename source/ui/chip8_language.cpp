#include "chip8_language.h"

#include "assembler/lexer.h"

#include <string>

namespace c8 {

namespace {

using Color = TextEditor::Color;
using Iterator = TextEditor::Iterator;

// The same separators as tokenize_line, and the start of a comment
auto ends_word(ImWchar c) -> bool {
    return (c == ' ') || (c == '\t') || (c == ',') || (c == ';');
}


// Color one word. The editor calls this at the start of each token that is
// not a space or a comment.
auto tokenize(Iterator start, Iterator end, Color& color) -> Iterator {
    auto word = std::string{};
    bool ascii = true;

    auto it = start;
    while ((it != end) && !ends_word(*it)) {
        const ImWchar c = *it;
        ascii = ascii && (c < 0x80);
        word.push_back(static_cast<char>(c));
        ++it;
    }

    // A ',' is punctuation. The editor colors it.
    if (it == start) {
        return start;
    }

    if (!ascii) {
        color = Color::text;
        return it;
    }

    // The highlighter does not know the position of the word in its line, so
    // a bare hex operand that is also an instruction name (add, bcd) shows as
    // an instruction name.
    switch (classify_word(word)) {
        case WordKind::mnemonic:         color = Color::keyword; break;
        case WordKind::directive:        color = Color::preprocessor; break;
        case WordKind::v_register:
        case WordKind::i_register:       color = Color::knownIdentifier; break;
        case WordKind::number:           color = Color::number; break;
        case WordKind::label:            color = Color::identifier; break;
        case WordKind::label_definition: color = Color::declaration; break;
        case WordKind::invalid:          color = Color::text; break;
    }

    return it;
}


auto make_language() -> TextEditor::Language {
    auto language = TextEditor::Language{};

    language.name = "CHIP-8";
    language.caseSensitive = false;
    language.singleLineComment = ";";
    language.isPunctuation = [](ImWchar c) { return (c == ',') || (c == ':'); };
    language.customTokenizer = tokenize;

    return language;
}

} // namespace


auto chip8_language() -> const TextEditor::Language* {
    static const auto language = make_language();
    return &language;
}

} // namespace c8
