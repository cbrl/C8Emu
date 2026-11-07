#include "quirks.h"

#include <algorithm>

namespace c8 {

auto find_preset(const Quirks& quirks) noexcept -> const Preset* {
    const auto it = std::ranges::find(presets, quirks, &Preset::quirks);
    return (it != presets.end()) ? &*it : nullptr;
}


auto find_preset(std::string_view id) noexcept -> const Preset* {
    const auto it = std::ranges::find(presets, id, &Preset::id);
    return (it != presets.end()) ? &*it : nullptr;
}

} // namespace c8
