#pragma once

#include <cstdint>
#include <expected>
#include <optional>
#include <string>
#include <string_view>

#include "ui/panel.h"

namespace c8 {

/// The address and the label at or before it, for example "0x0204  loop+0x4"
[[nodiscard]]
auto address_with_name(const UiContext& context, uint32_t address) -> std::string;

/**
 * @brief The address of a debugger expression, for an address field
 *
 * @details The text can be a number, a label, or an expression, for example `loop+4`.
 *
 * @return The address, or a message that tells why the text is not an address
 */
[[nodiscard]]
auto evaluate_address(const UiContext& context, std::string_view text) -> std::expected<uint16_t, std::string>;

/**
 * @brief The items of the menu of an address
 *
 * @details The items show the address in the other panels, toggle a
 *          breakpoint, run to the address, set the PC, watch the byte, and
 *          copy the address. Each panel that lists addresses uses these items,
 *          so all menus are the same.
 *
 * @param[in] here  The panel of the menu. The menu has no item that shows the address in this panel.
 * @param[in] size  The bytes of the breakpoint toggle. A source line can write more than one instruction.
 */
auto draw_address_menu_items(UiContext& context, uint32_t address, std::optional<ShowTarget> here, uint16_t size = 1) -> void;

/// A menu of an address. A right click on the last item opens it. The first line of the menu shows the address and its label.
auto address_context_menu(UiContext& context, uint32_t address, std::optional<ShowTarget> here) -> void;

} // namespace c8
