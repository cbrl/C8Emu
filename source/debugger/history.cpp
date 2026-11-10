#include "history.h"

#include "chip8/serialize.h"

#include <algorithm>
#include <optional>
#include <span>
#include <utility>

namespace c8 {

namespace {

// Write a count as little-endian base-128 (LEB128)
auto append_count(std::vector<uint8_t>& out, size_t count) -> void {
    do {
        auto byte = static_cast<uint8_t>(count & 0x7F);
        count >>= 7;
        if (count != 0) {
            byte = static_cast<uint8_t>(byte | 0x80);
        }
        out.push_back(byte);
    } while (count != 0);
}


auto read_count(std::span<const uint8_t> in, size_t& pos) -> std::optional<size_t> {
    size_t value = 0;
    for (int shift = 0; shift < 35; shift += 7) {
        if (pos >= in.size()) {
            return std::nullopt;
        }
        const uint8_t byte = in[pos++];
        value |= static_cast<size_t>(byte & 0x7F) << shift;
        if ((byte & 0x80) == 0) {
            return value;
        }
    }
    return std::nullopt;
}


// Compress bytes with long runs of zeros, for example the XOR of two similar states.
// The output is pairs of (zero count, literal count), each pair followed by the literal bytes.
auto compress_zero_runs(std::span<const uint8_t> bytes) -> std::vector<uint8_t> {
    auto out = std::vector<uint8_t>{};
    size_t pos = 0;

    while (pos < bytes.size()) {
        size_t zeros = 0;
        while (((pos + zeros) < bytes.size()) && (bytes[pos + zeros] == 0)) {
            ++zeros;
        }
        pos += zeros;

        // A literal run ends at 4 or more zeros, which cost less as a zero run
        size_t literal = 0;
        while ((pos + literal) < bytes.size()) {
            size_t run = 0;
            while (((pos + literal + run) < bytes.size()) && (bytes[pos + literal + run] == 0) && (run < 4)) {
                ++run;
            }
            if ((run >= 4) || ((pos + literal + run) == bytes.size())) {
                break;
            }
            literal += run + 1;
        }

        append_count(out, zeros);
        append_count(out, literal);
        out.insert(out.end(), bytes.begin() + static_cast<ptrdiff_t>(pos), bytes.begin() + static_cast<ptrdiff_t>(pos + literal));
        pos += literal;
    }

    return out;
}


// Undo compress_zero_runs(). Returns std::nullopt for bad data.
auto expand_zero_runs(std::span<const uint8_t> packed) -> std::optional<std::vector<uint8_t>> {
    auto out = std::vector<uint8_t>{};
    size_t pos = 0;

    while (pos < packed.size()) {
        const auto zeros = read_count(packed, pos);
        const auto literal = read_count(packed, pos);
        if (!zeros || !literal || ((packed.size() - pos) < *literal)) {
            return std::nullopt;
        }
        out.insert(out.end(), *zeros, uint8_t{0});
        out.insert(out.end(), packed.begin() + static_cast<ptrdiff_t>(pos), packed.begin() + static_cast<ptrdiff_t>(pos + *literal));
        pos += *literal;
    }

    return out;
}

} // namespace


History::History(size_t capacity) : max_states(std::max<size_t>(capacity, 1)) {
}


auto History::clear() -> void {
    newest.clear();
    newest_cycles = 0;
    older.clear();
    key_journal.clear();
}


auto History::set_capacity(size_t frames) -> void {
    max_states = std::max<size_t>(frames, 1);
    while (size() > max_states) {
        older.pop_front();
    }
}


auto History::size() const noexcept -> size_t {
    return newest.empty() ? 0 : (older.size() + 1);
}


auto History::memory_use() const noexcept -> size_t {
    size_t total = newest.capacity() + incoming.capacity() + difference.capacity();
    for (const auto& entry : older) {
        total += entry.data.capacity() + sizeof(Older);
    }
    return total;
}


auto History::has_state_before(uint64_t total_cycles) const noexcept -> bool {
    if (newest.empty()) {
        return false;
    }
    const uint64_t oldest = older.empty() ? newest_cycles : older.front().total_cycles;
    return oldest < total_cycles;
}


auto History::record(const MachineState& state) -> void {
    serialize_state(state, incoming);

    if (!newest.empty()) {
        auto entry = Older{.total_cycles = newest_cycles};

        if (newest.size() == incoming.size()) {
            // The newest state becomes the XOR difference to the incoming state
            difference.resize(newest.size());
            for (size_t index = 0; index < newest.size(); ++index) {
                difference[index] = static_cast<uint8_t>(newest[index] ^ incoming[index]);
            }
            entry.data = compress_zero_runs(difference);
        }
        else {
            // The display size changed, so the states have different sizes
            entry.whole = true;
            entry.data = compress_zero_runs(newest);
        }

        older.push_back(std::move(entry));
        while (size() > max_states) {
            older.pop_front();
        }
    }

    std::swap(newest, incoming);
    newest_cycles = state.total_cycles;

    // A replay never starts before the oldest state, so the journal needs only the newest change before it
    const uint64_t oldest = older.empty() ? newest_cycles : older.front().total_cycles;
    while ((key_journal.size() > 1) && (key_journal[1].first <= oldest)) {
        key_journal.pop_front();
    }
}


auto History::drop_newest() -> bool {
    if (older.empty()) {
        newest.clear();
        newest_cycles = 0;
        return true;
    }

    auto& entry = older.back();
    auto data = expand_zero_runs(entry.data);
    if (!data) {
        return false;
    }

    if (entry.whole) {
        newest = std::move(*data);
    }
    else if (data->size() == newest.size()) {
        for (size_t index = 0; index < newest.size(); ++index) {
            newest[index] = static_cast<uint8_t>(newest[index] ^ (*data)[index]);
        }
    }
    else {
        return false;
    }

    newest_cycles = entry.total_cycles;
    older.pop_back();
    return true;
}


auto History::rewind_before(uint64_t total_cycles) -> std::optional<MachineState> {
    if (!has_state_before(total_cycles)) {
        return std::nullopt;
    }

    while (newest_cycles >= total_cycles) {
        if (!drop_newest()) {
            clear();
            return std::nullopt;
        }
    }

    auto state = deserialize_state(newest);
    if (!state) {
        clear();
        return std::nullopt;
    }
    return std::move(*state);
}


auto History::note_keys(uint64_t total_cycles, uint16_t keys) -> void {
    if (!key_journal.empty() && (key_journal.back().first == total_cycles)) {
        key_journal.back().second = keys;
        return;
    }
    key_journal.emplace_back(total_cycles, keys);
}


auto History::discard_keys_after(uint64_t total_cycles) -> void {
    while (!key_journal.empty() && (key_journal.back().first > total_cycles)) {
        key_journal.pop_back();
    }
}


auto History::keys_at(uint64_t total_cycles, uint64_t since) const -> std::optional<uint16_t> {
    // A change at `since` came after the state at `since` was recorded, or it holds the same keys as that state
    for (auto it = key_journal.rbegin(); it != key_journal.rend(); ++it) {
        if (it->first <= total_cycles) {
            if (it->first >= since) {
                return it->second;
            }
            return std::nullopt;
        }
    }
    return std::nullopt;
}

} // namespace c8
