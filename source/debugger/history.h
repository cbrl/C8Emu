#pragma once

#include <cstddef>
#include <cstdint>
#include <deque>
#include <optional>
#include <utility>
#include <vector>

#include "chip8/machine_state.h"

namespace c8 {

/**
 * @class History
 *
 * @brief The machine states at past frame ends, for rewind and step back
 *
 * @details The newest state is stored whole. Each older state is stored as
 *          the compressed XOR difference to the state after it. Most bytes of
 *          two frames are the same, so a typical frame needs a few hundred
 *          bytes. The removal of the newest state costs one difference.
 *
 *          The history also keeps a journal of key changes. A replay from a
 *          past state uses it to press the same keys at the same instructions.
 */
class History {
public:
    /// One minute of frames
    static constexpr size_t default_capacity = 60 * 60;

    explicit History(size_t capacity = default_capacity);

    /// Remove all states and key changes
    auto clear() -> void;

    /// Set the largest number of states. The oldest states go first.
    auto set_capacity(size_t frames) -> void;

    [[nodiscard]]
    auto capacity() const noexcept -> size_t {
        return max_states;
    }

    /// Add a state, normally the state at the end of a frame
    auto record(const MachineState& state) -> void;

    /// The number of states
    [[nodiscard]]
    auto size() const noexcept -> size_t;

    [[nodiscard]]
    auto empty() const noexcept -> bool {
        return newest.empty();
    }

    /// The approximate memory use in bytes
    [[nodiscard]]
    auto memory_use() const noexcept -> size_t;

    /// True if a state is older than an instruction count
    [[nodiscard]]
    auto has_state_before(uint64_t total_cycles) const noexcept -> bool;

    /**
     * @brief Go back to the newest state before an instruction count
     *
     * @details The states at or after `total_cycles` are removed, because the
     *          machine goes back before them. If no older state exists,
     *          nothing changes and the result is std::nullopt. Damaged data
     *          clears the history.
     */
    [[nodiscard]]
    auto rewind_before(uint64_t total_cycles) -> std::optional<MachineState>;

    /// Note that the keys changed before the instruction with this count
    auto note_keys(uint64_t total_cycles, uint16_t keys) -> void;

    /// Remove the key changes after an instruction count, because the machine went back before them
    auto discard_keys_after(uint64_t total_cycles) -> void;

    /**
     * @brief The keys of the journal at an instruction count
     *
     * @details The result is the newest change at or before `total_cycles`.
     *          It is std::nullopt if that change is before `since` (the count
     *          of the state that a replay starts from): the state holds the
     *          keys then.
     */
    [[nodiscard]]
    auto keys_at(uint64_t total_cycles, uint64_t since) const -> std::optional<uint16_t>;

private:
    struct Older {
        bool whole = false;          // The data is the compressed state, not a difference
        uint64_t total_cycles = 0;
        std::vector<uint8_t> data;   // Compressed
    };

    // Remove the newest state. The state before it becomes the newest. Returns false for damaged data.
    auto drop_newest() -> bool;

    size_t max_states;

    std::vector<uint8_t> newest;     // The serialized newest state. Empty if the history is empty.
    uint64_t newest_cycles = 0;
    std::deque<Older> older;         // Oldest first

    std::deque<std::pair<uint64_t, uint16_t>> key_journal; // (instruction count, keys), oldest first

    // Buffers that record() uses again, so a frame needs no large allocation
    std::vector<uint8_t> incoming;
    std::vector<uint8_t> difference;
};

} // namespace c8
