#pragma once

#include <cstdint>
#include <string>

namespace c8 {

/// How serious a diagnostic is
enum class Severity : uint8_t {
    error,   ///< The program cannot load
    warning, ///< The program loads, but the line is probably wrong
};


/**
 * @struct Diagnostic
 *
 * @brief A message about one place in a source text
 *
 * @details The line and the column are zero-based, the same as in the code
 *          editor. The column and the length count code points, not bytes.
 */
struct Diagnostic {
    Severity    severity = Severity::error;
    uint32_t    line     = 0;
    uint32_t    column   = 0;
    uint32_t    length   = 0;
    std::string message;

    [[nodiscard]]
    auto operator==(const Diagnostic&) const -> bool = default;
};

} // namespace c8
