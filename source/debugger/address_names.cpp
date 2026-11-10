#include "address_names.h"

#include <algorithm>
#include <format>

namespace c8 {

auto address_name(uint32_t address, std::span<const Label> labels) -> std::string {
    // The first label after the address. The label before it is the nearest one at or before the address.
    const auto after = std::ranges::upper_bound(labels, static_cast<int64_t>(address), {}, &Label::value);
    if (after == labels.begin()) {
        return {};
    }

    const auto& label = *(after - 1);
    const auto offset = static_cast<int64_t>(address) - label.value;
    return (offset == 0) ? label.name : std::format("{}+0x{:X}", label.name, offset);
}

} // namespace c8
