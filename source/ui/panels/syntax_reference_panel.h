#pragma once

#include <optional>

#include <imgui.h>

#include "chip8/variant.h"
#include "ui/panel.h"

namespace c8 {

/**
 * @class SyntaxReferencePanel
 *
 * @brief The instructions, directives, and literals of the assembler
 *
 * @details The instruction table comes from the same forms table as the
 *          assembler, so it always matches what the assembler accepts.
 */
class SyntaxReferencePanel final : public Panel {
public:
    [[nodiscard]]
    auto title() const -> const char* override {
        return "Syntax Reference";
    }

    auto draw(UiContext& context) -> void override;

private:
    ImGuiTextFilter filter;

    // Show only the instructions of this platform. std::nullopt shows all.
    std::optional<Variant> platform;
};

} // namespace c8
