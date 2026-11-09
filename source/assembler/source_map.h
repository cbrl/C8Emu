#pragma once

#include <cstdint>
#include <span>
#include <vector>

namespace c8 {

/// What the bytes of a source span hold
enum class SpanKind : uint8_t {
    code, ///< An instruction
    data, ///< Bytes or words from db, dw, or a lone number
};

/// The bytes that one source line writes
struct SourceSpan {
    uint32_t line    = 0; ///< Zero-based
    uint16_t address = 0;
    uint16_t size    = 0;
    SpanKind kind    = SpanKind::code;

    [[nodiscard]]
    auto operator==(const SourceSpan&) const -> bool = default;
};


/**
 * @class SourceMap
 *
 * @brief Connects source lines and program addresses
 *
 * @details The spans are in order of address and of line.
 */
class SourceMap {
public:
    [[nodiscard]]
    auto operator==(const SourceMap&) const -> bool = default;

    /// Add a span after the last span. A span with no bytes is ignored.
    auto add(const SourceSpan& span) -> void;

    [[nodiscard]]
    auto spans() const noexcept -> std::span<const SourceSpan> {
        return span_list;
    }

    [[nodiscard]]
    auto empty() const noexcept -> bool {
        return span_list.empty();
    }

    /// The span whose bytes hold an address. nullptr if no span holds it.
    [[nodiscard]]
    auto span_containing(uint16_t address) const noexcept -> const SourceSpan*;

    /**
     * @brief The span of a line
     *
     * @param[in] line          The zero-based line
     * @param[in] snap_forward  If the line is not an instruction (a comment, a label, a blank
     *                          line), give the next span after it
     *
     * @return The span, or nullptr if there is none
     */
    [[nodiscard]]
    auto span_for_line(uint32_t line, bool snap_forward) const noexcept -> const SourceSpan*;

private:
    std::vector<SourceSpan> span_list;
};

} // namespace c8
