#include "line_tracker.h"

#include "assembler/lexer.h"

#include <algorithm>
#include <span>
#include <utility>

namespace c8 {

namespace {

// The line diff stops after this many inserted and deleted lines. A larger
// change pairs its changed lines in order, with no equal lines.
constexpr int max_diff_edits = 1000;

// The pairing of changed lines uses a table of this many cells at most. A
// larger change pairs its lines in order.
constexpr size_t max_pairing_cells = size_t{1} << 20;

// The score of a pair of changed lines. The same instruction name scores more,
// so an edited line keeps its tag when a new line comes before it.
constexpr uint32_t pair_score      = 1;
constexpr uint32_t same_name_score = 3;


// The instruction name or directive of a line, in lowercase. Empty if the line
// writes no bytes: a blank line, a comment, a label alone, org, align,
// platform, or a constant (name equ value).
auto statement_key(std::string_view line) -> std::string {
    const auto tokens = tokenize_line(line);

    // The assembler reads a first word that ends with ':' as a label
    const size_t index = statement_start(tokens);
    if ((index >= tokens.size()) || defines_constant(tokens, index)) {
        return {};
    }

    auto key = to_lower_ascii(tokens[index].text);
    const bool no_bytes = (key == "org") || (key == "align") || (key == "platform") || (key == "equ");
    return no_bytes ? std::string{} : key;
}


struct LinePair {
    uint32_t before = 0;
    uint32_t after  = 0;
};

/**
 * The equal lines of a shortest edit from `before` to `after` (Myers' greedy
 * algorithm). The pairs are in order. std::nullopt if the edit needs more than
 * max_diff_edits inserted and deleted lines.
 */
auto equal_line_pairs(std::span<const std::string> before, std::span<const std::string_view> after)
    -> std::optional<std::vector<LinePair>>
{
    const int n = static_cast<int>(before.size());
    const int m = static_cast<int>(after.size());
    const int limit = std::min(n + m, max_diff_edits);

    // furthest[k] is the largest x on diagonal k = x - y, for k in [-limit - 1, limit + 1]
    const int offset = limit + 1;
    auto furthest_x = std::vector<int>(static_cast<size_t>((2 * limit) + 3), 0);
    const auto furthest = [&](int k) -> int& { return furthest_x[static_cast<size_t>(offset + k)]; };

    // trace[d] holds furthest[k] for k in [-d - 1, d + 1] before step d
    auto trace = std::vector<std::vector<int>>{};

    int edits = -1;
    for (int d = 0; (d <= limit) && (edits < 0); ++d) {
        const auto window = furthest_x.begin() + (offset - d - 1);
        trace.emplace_back(window, window + ((2 * d) + 3));

        for (int k = -d; k <= d; k += 2) {
            const bool down = (k == -d) || ((k != d) && (furthest(k - 1) < furthest(k + 1)));
            int x = down ? furthest(k + 1) : (furthest(k - 1) + 1);
            int y = x - k;

            while ((x < n) && (y < m) && (before[static_cast<size_t>(x)] == after[static_cast<size_t>(y)])) {
                ++x;
                ++y;
            }
            furthest(k) = x;

            if ((x >= n) && (y >= m)) {
                edits = d;
                break;
            }
        }
    }

    if (edits < 0) {
        return std::nullopt;
    }

    // Walk back from the end. Each diagonal move is a pair of equal lines.
    auto pairs = std::vector<LinePair>{};
    int x = n;
    int y = m;

    const auto add_pairs_to = [&](int stop_x, int stop_y) {
        while ((x > stop_x) && (y > stop_y)) {
            --x;
            --y;
            pairs.push_back({static_cast<uint32_t>(x), static_cast<uint32_t>(y)});
        }
    };

    for (int d = edits; d > 0; --d) {
        const auto& old_v = trace[static_cast<size_t>(d)];
        const auto at = [&](int k) { return old_v[static_cast<size_t>(k + d + 1)]; };

        const int k = x - y;
        const bool down = (k == -d) || ((k != d) && (at(k - 1) < at(k + 1)));
        const int previous_k = down ? (k + 1) : (k - 1);
        const int previous_x = at(previous_k);
        const int previous_y = previous_x - previous_k;

        add_pairs_to(previous_x, previous_y);
        x = previous_x;
        y = previous_y;
    }
    add_pairs_to(0, 0);

    std::ranges::reverse(pairs);
    return pairs;
}

} // namespace


auto LineTracker::load(std::string_view text, SourceMap program_map) -> void {
    loaded = true;
    map = std::move(program_map);

    const auto lines = split_lines(text);
    const auto count = lines.size();

    loaded_lines.assign(lines.begin(), lines.end());

    loaded_keys.clear();
    loaded_keys.reserve(count);
    for (const auto line : lines) {
        loaded_keys.push_back(statement_key(line));
    }

    loaded_span_of_line.assign(count, none);
    const auto spans = map.spans();
    for (size_t index = 0; index < spans.size(); ++index) {
        if (spans[index].line < count) {
            loaded_span_of_line[spans[index].line] = static_cast<uint32_t>(index);
        }
    }

    // A loaded line that wrote no bytes, for example `ds 0`, is not a statement. A breakpoint goes past it.
    for (size_t line = 0; line < count; ++line) {
        if (loaded_span_of_line[line] == none) {
            loaded_keys[line].clear();
        }
    }

    update(text);
}


auto LineTracker::clear() -> void {
    loaded = false;
    unchanged = false;
    map = {};

    loaded_lines.clear();
    loaded_span_of_line.clear();
    loaded_keys.clear();

    span_of_line.clear();
    line_of_span.clear();
    statement_lines.clear();
}


auto LineTracker::update(std::string_view text) -> void {
    if (!loaded) {
        return;
    }

    const auto lines = split_lines(text);
    const auto count = static_cast<uint32_t>(lines.size());
    const auto loaded_count = static_cast<uint32_t>(loaded_lines.size());

    span_of_line.assign(count, none);
    line_of_span.assign(map.spans().size(), none);
    statement_lines.assign(count, false);

    // The equal lines at the start and at the end need no diff
    uint32_t head = 0;
    while ((head < loaded_count) && (head < count) && (loaded_lines[head] == lines[head])) {
        ++head;
    }

    uint32_t tail = 0;
    while ((tail < (loaded_count - head)) && (tail < (count - head))
        && (loaded_lines[loaded_count - 1 - tail] == lines[count - 1 - tail]))
    {
        ++tail;
    }

    unchanged = (head == loaded_count) && (head == count);

    for (uint32_t line = 0; line < head; ++line) {
        keep_line(line, line);
    }
    for (uint32_t i = 0; i < tail; ++i) {
        keep_line(loaded_count - 1 - i, count - 1 - i);
    }

    const auto loaded_middle = std::span{loaded_lines}.subspan(head, loaded_count - head - tail);
    const auto middle = std::span{lines}.subspan(head, count - head - tail);
    const auto pairs = equal_line_pairs(loaded_middle, middle).value_or(std::vector<LinePair>{});

    // Pair the changed lines in each gap between two equal lines
    uint32_t from = head;
    uint32_t to = head;

    for (const auto& pair : pairs) {
        const uint32_t loaded_line = head + pair.before;
        const uint32_t line = head + pair.after;

        pair_changed_lines(from, loaded_line, to, line, lines);
        keep_line(loaded_line, line);

        from = loaded_line + 1;
        to = line + 1;
    }
    pair_changed_lines(from, loaded_count - tail, to, count - tail, lines);
}


auto LineTracker::span_at(uint32_t line) const noexcept -> const SourceSpan* {
    if ((line >= span_of_line.size()) || (span_of_line[line] == none)) {
        return nullptr;
    }
    return &map.spans()[span_of_line[line]];
}


auto LineTracker::line_of(uint16_t address) const noexcept -> std::optional<uint32_t> {
    const auto* span = map.span_containing(address);
    if (span == nullptr) {
        return std::nullopt;
    }

    const auto index = static_cast<size_t>(span - map.spans().data());
    if ((index >= line_of_span.size()) || (line_of_span[index] == none)) {
        return std::nullopt;
    }
    return line_of_span[index];
}


auto LineTracker::statement_line_from(uint32_t line) const noexcept -> std::optional<uint32_t> {
    for (auto index = static_cast<size_t>(line); index < statement_lines.size(); ++index) {
        if (statement_lines[index]) {
            return static_cast<uint32_t>(index);
        }
    }
    return std::nullopt;
}


auto LineTracker::keep_line(uint32_t from, uint32_t to) -> void {
    statement_lines[to] = !loaded_keys[from].empty();
    move_tag(from, to);
}


auto LineTracker::move_tag(uint32_t from, uint32_t to) -> void {
    const auto span = loaded_span_of_line[from];
    if (span == none) {
        return;
    }
    span_of_line[to] = span;
    line_of_span[span] = to;
}


auto LineTracker::pair_changed_lines(
	uint32_t from_begin,
	uint32_t from_end,
	uint32_t to_begin,
	uint32_t to_end,
    const std::vector<std::string_view>& lines) -> void
{
    struct Candidate {
        uint32_t line;
        std::string key;
    };

    // The new lines with a statement can take a tag
    auto targets = std::vector<Candidate>{};
    for (auto line = to_begin; line < to_end; ++line) {
        auto key = statement_key(lines[line]);
        if (!key.empty()) {
            statement_lines[line] = true;
            targets.push_back({line, std::move(key)});
        }
    }

    // The changed loaded lines with a tag can give it
    auto sources = std::vector<Candidate>{};
    for (auto line = from_begin; line < from_end; ++line) {
        if (loaded_span_of_line[line] != none) {
            sources.push_back({line, loaded_keys[line]});
        }
    }

    const size_t p = sources.size();
    const size_t q = targets.size();
    if ((p == 0) || (q == 0)) {
        return;
    }

    // A large change pairs the lines in order
    if ((p + 1) * (q + 1) > max_pairing_cells) {
        for (size_t i = 0; i < std::min(p, q); ++i) {
            move_tag(sources[i].line, targets[i].line);
        }
        return;
    }

    const auto score = [&](size_t i, size_t j) {
        return (sources[i].key == targets[j].key) ? same_name_score : pair_score;
    };

    // best[i][j] is the highest score for the sources from i and the targets from j
    const size_t width = q + 1;
    auto best = std::vector<uint32_t>((p + 1) * width, 0);
    const auto cell = [&](size_t i, size_t j) -> uint32_t& { return best[(i * width) + j]; };

    for (size_t i = p; i-- > 0;) {
        for (size_t j = q; j-- > 0;) {
            cell(i, j) = std::max({score(i, j) + cell(i + 1, j + 1), cell(i + 1, j), cell(i, j + 1)});
        }
    }

    // Take the pairs from the start. On a tie, pair the earliest lines.
    size_t i = 0;
    size_t j = 0;
    while ((i < p) && (j < q)) {
        if (cell(i, j) == score(i, j) + cell(i + 1, j + 1)) {
            move_tag(sources[i].line, targets[j].line);
            ++i;
            ++j;
        }
        else if (cell(i, j) == cell(i + 1, j)) {
            ++i;
        }
        else {
            ++j;
        }
    }
}

} // namespace c8
