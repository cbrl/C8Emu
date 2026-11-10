#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "assembler/diagnostic.h"
#include "assembler/source_map.h"
#include "chip8/variant.h"

namespace c8 {

struct AssembleOptions {
    uint16_t origin  = 0x200;          ///< The address of the first program byte
    Variant  variant = Variant::chip8; ///< The platform if the source has no `platform` line
};

/// What a name of the source stands for
enum class LabelKind : uint8_t {
    label,    ///< `name:`, the address of a line
    constant, ///< `name equ value`
};

/// A label or a constant, and its value
struct Label {
    std::string name;                    ///< Lowercase
    LabelKind   kind  = LabelKind::label;
    int64_t     value = 0;               ///< The address of a label, or the value of a constant
    bool        known = true;            ///< False for a constant whose value has an error. Its value is then 0.
    uint32_t    line  = 0;               ///< The zero-based line of the definition

    [[nodiscard]]
    auto operator==(const Label&) const -> bool = default;
};


struct AssembleResult {
    uint16_t origin = 0x200;

    /// The platform of the program: the `platform` line, or the default of the options
    Variant variant = Variant::chip8;

    /// The program bytes, from the origin. Not usable when there are errors.
    std::vector<uint8_t> image;

    SourceMap map;

    /// The labels and the constants, in order of definition
    std::vector<Label> labels;

    /// The errors and warnings, in order of line and column
    std::vector<Diagnostic> diagnostics;

    /// True if there are no errors. Warnings are allowed.
    [[nodiscard]]
    auto ok() const noexcept -> bool {
        return error_count() == 0;
    }

    [[nodiscard]]
    auto error_count() const noexcept -> size_t;

    [[nodiscard]]
    auto warning_count() const noexcept -> size_t;
};


/**
 * @brief Assemble a CHIP-8 program
 *
 * @details One line holds `[label:] [instruction or directive] [; comment]`.
 *          Spaces, tabs, and commas separate the words. Letter case has no
 *          effect. The directives are:
 *          - `db value, ...`: write bytes
 *          - `dw value, ...`: write 16-bit words, high byte first
 *          - `ds count [fill]`: write count bytes of the fill value (default 0)
 *          - `org address`: move forward to an address. Zeros fill the gap.
 *          - `align size`: move forward to a multiple of size, a power of two
 *          - `name equ value`: define a constant
 *          - `platform name`: the platform (chip8, hires, schip, or xochip). It
 *            must come before the first instruction, data, or org line. The
 *            platform sets the instructions and the memory size.
 *
 *          A line with only a number writes that number as a word.
 *
 *          A value can be an expression (see parse_expression). An expression
 *          has no spaces, or it is in parentheses: `x+1` or `(x + 1)`. So a
 *          space always separates two operands.
 *
 *          The assembler makes two passes. The first pass finds the address of
 *          each line and defines the labels. The second pass writes the bytes.
 *          A label or a constant can be used before its line. Only org, ds, and
 *          align need their value in the first pass, so they can use only the
 *          names before them. A line with an error still takes its space, so the
 *          addresses of the lines after it do not change.
 */
[[nodiscard]]
auto assemble(std::string_view source, const AssembleOptions& options = {}) -> AssembleResult;

} // namespace c8
