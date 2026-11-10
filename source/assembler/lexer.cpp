#include "lexer.h"

#include "assembler/forms.h"

#include <algorithm>
#include <array>
#include <format>

namespace c8 {

namespace {

constexpr auto lower(char c) noexcept -> char {
    return ((c >= 'A') && (c <= 'Z')) ? static_cast<char>(c - 'A' + 'a') : c;
}

constexpr auto digit_value(char c) noexcept -> int {
    c = lower(c);
    if ((c >= '0') && (c <= '9')) {
        return c - '0';
    }
    if ((c >= 'a') && (c <= 'f')) {
        return c - 'a' + 10;
    }
    return -1;
}

auto all_hex_digits(std::string_view text) noexcept -> bool {
    return !text.empty() && std::ranges::all_of(text, is_hex_digit);
}

constexpr auto is_label_start(char c) noexcept -> bool {
    c = lower(c);
    return ((c >= 'a') && (c <= 'z')) || (c == '_') || (c == '.');
}

// The form of a label name, without the reserved words
auto has_label_syntax(std::string_view name) noexcept -> bool {
    return !name.empty() && is_label_start(name.front()) && std::ranges::all_of(name, is_label_char);
}

} // namespace


auto is_label_char(char c) noexcept -> bool {
    return is_label_start(c) || ((c >= '0') && (c <= '9'));
}


auto has_number_prefix(std::string_view word) noexcept -> bool {
    if ((word.size() >= 2) && (word[0] == '0') && (lower(word[1]) == 'x')) {
        return true;
    }
    return !word.empty() && ((word[0] == '#') || (word[0] == '%'));
}


auto statement_start(std::span<const Token> tokens) noexcept -> size_t {
    return (!tokens.empty() && tokens[0].text.ends_with(':')) ? 1 : 0;
}


auto defines_constant(std::span<const Token> tokens, size_t head) -> bool {
    return ((head + 1) < tokens.size()) && (to_lower_ascii(tokens[head + 1].text) == "equ");
}


auto reserved_word(std::string_view word) -> std::optional<ReservedWord> {
    const auto lowercase = to_lower_ascii(word);

    if ((lowercase == "i") || parse_v_register(word)) {
        return ReservedWord::register_name;
    }
    if (lowercase == "long") {
        return ReservedWord::keyword;
    }
    if (is_mnemonic(lowercase)) {
        return ReservedWord::mnemonic;
    }
    if (is_directive(lowercase)) {
        return ReservedWord::directive;
    }
    return std::nullopt;
}


auto tokenize_line(std::string_view line) -> std::vector<Token> {
    auto tokens = std::vector<Token>{};

    uint32_t column = 0;
    size_t pos = 0;

    while (pos < line.size()) {
        const char c = line[pos];

        if (c == ';') {
            break;
        }

        if (is_separator(c)) {
            if (starts_code_point(c)) {
                ++column;
            }
            ++pos;
            continue;
        }

        // A word runs to the next separator or comment. Inside parentheses, a separator is part of the word.
        const size_t start = pos;
        const uint32_t start_column = column;
        int depth = 0;

        while ((pos < line.size()) && (line[pos] != ';') && ((depth > 0) || !is_separator(line[pos]))) {
            if (line[pos] == '(') {
                ++depth;
            }
            else if ((line[pos] == ')') && (depth > 0)) {
                --depth;
            }

            if (starts_code_point(line[pos])) {
                ++column;
            }
            ++pos;
        }

        // A '(' without a ')' takes the rest of the line. The separators at the end are not part of the word.
        if (depth > 0) {
            while ((pos > start) && is_separator(line[pos - 1])) {
                --pos;
                --column;
            }
        }

        tokens.push_back(Token{
            .text   = line.substr(start, pos - start),
            .column = start_column,
            .length = column - start_column,
        });
    }

    return tokens;
}


auto split_lines(std::string_view text) -> std::vector<std::string_view> {
    auto lines = std::vector<std::string_view>{};

    while (true) {
        const auto end = text.find('\n');
        auto line = text.substr(0, end);

        if (line.ends_with('\r')) {
            line.remove_suffix(1);
        }
        lines.push_back(line);

        if (end == std::string_view::npos) {
            break;
        }
        text.remove_prefix(end + 1);
    }

    return lines;
}


auto to_lower_ascii(std::string_view word) -> std::string {
    auto result = std::string{word};
    for (auto& c : result) {
        c = lower(c);
    }
    return result;
}


auto parse_v_register(std::string_view word) noexcept -> std::optional<uint32_t> {
    if ((word.size() < 2) || (lower(word[0]) != 'v')) {
        return std::nullopt;
    }

    const auto digits = word.substr(1);
    if (!all_hex_digits(digits)) {
        return std::nullopt;
    }

    // Many digits are an error later. Keep the value small, so it does not overflow.
    uint32_t value = 0;
    for (const char c : digits) {
        value = std::min<uint32_t>((value * 16) + static_cast<uint32_t>(digit_value(c)), 0x10000);
    }
    return value;
}


auto classify_word(std::string_view word, WordPosition position) noexcept -> WordKind {
    if (word.empty()) {
        return WordKind::invalid;
    }

    if (word.back() == ':') {
        return has_label_syntax(word.substr(0, word.size() - 1)) ? WordKind::label_definition : WordKind::invalid;
    }

    // Mnemonics and directives have a known maximum length, so this fixed buffer will hold any of them.
    auto buffer = std::array<char, 8>{};
    auto lowercase = std::string_view{};
    if (word.size() <= buffer.size()) {
        for (size_t i = 0; i < word.size(); ++i) {
            buffer[i] = lower(word[i]);
        }
        lowercase = std::string_view{buffer.data(), word.size()};
    }

    if (lowercase == "i") {
        return WordKind::i_register;
    }
    if (lowercase == "long") {
        return WordKind::long_keyword;
    }
    if (parse_v_register(word)) {
        return WordKind::v_register;
    }

    const bool number = has_number_prefix(word) || all_hex_digits(word);

    if ((position == WordPosition::operand) && number) {
        return WordKind::number;
    }
    if (is_mnemonic(lowercase)) {
        return WordKind::mnemonic;
    }
    if (is_directive(lowercase)) {
        return WordKind::directive;
    }
    if (number) {
        return WordKind::number;
    }
    if (has_label_syntax(word)) {
        return WordKind::label;
    }

    return WordKind::invalid;
}


auto parse_number(std::string_view word) -> std::expected<int64_t, std::string> {
    int base = 16;
    bool negative = false;
    bool separators = true;
    auto digits = word;

    if (word.starts_with('#')) {
        base = 10;
        digits.remove_prefix(1);
        if (digits.starts_with('-')) {
            negative = true;
            digits.remove_prefix(1);
        }
    }
    else if (word.starts_with('%')) {
        base = 2;
        digits.remove_prefix(1);
    }
    else if ((word.size() >= 2) && (word[0] == '0') && (lower(word[1]) == 'x')) {
        digits.remove_prefix(2);
    }
    else {
        // A bare hex number has no '_': "dead_beef" is a label
        separators = false;
    }

    static constexpr uint64_t max_value = 0xFFFF'FFFF;

    uint64_t value = 0;
    bool has_digit = false;

    for (const char c : digits) {
        if ((c == '_') && separators) {
            continue;
        }

        // A sprite row: a '.' is 0 and a '#' is 1
        int digit = digit_value(c);
        if (base == 2) {
            if (c == '.') {
                digit = 0;
            }
            else if (c == '#') {
                digit = 1;
            }
        }

        if ((digit < 0) || (digit >= base)) {
            const auto* kind = (base == 16) ? "hexadecimal" : ((base == 10) ? "decimal" : "binary");
            return std::unexpected(std::format("'{}' is not a {} number", word, kind));
        }

        value = (value * static_cast<uint64_t>(base)) + static_cast<uint64_t>(digit);
        if (value > max_value) {
            return std::unexpected(std::format("'{}' is too large", word));
        }
        has_digit = true;
    }

    if (!has_digit) {
        return std::unexpected(std::format("'{}' has no digits", word));
    }

    const auto result = static_cast<int64_t>(value);
    return negative ? -result : result;
}


auto label_name_error(std::string_view name, std::string_view kind) -> std::optional<std::string> {
    if (!has_label_syntax(name)) {
        return std::format(
            "'{}' is not a valid {} name. A name starts with a letter, '_', or '.', and holds only letters, digits, '_', and '.'.",
            name, kind);
    }

    if (const auto reserved = reserved_word(name)) {
        switch (*reserved) {
            case ReservedWord::register_name: return std::format("'{}' is a register name, so it cannot be a {}", name, kind);
            case ReservedWord::keyword:       return std::format("'{}' is a keyword (mov i, long nnnn), so it cannot be a {}", name, kind);
            case ReservedWord::mnemonic:      return std::format("'{}' is an instruction name, so it cannot be a {}", name, kind);
            case ReservedWord::directive:     return std::format("'{}' is a directive name, so it cannot be a {}", name, kind);
        }
    }
    if (all_hex_digits(name)) {
        return std::format("'{}' reads as the hex number 0x{}, so it cannot be a {}", name, name, kind);
    }

    return std::nullopt;
}

} // namespace c8
