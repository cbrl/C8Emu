#include "quirks.h"

#include <algorithm>

namespace c8 {

auto find_preset(Variant variant, const Quirks& quirks) noexcept -> const Preset* {
    const auto it = std::ranges::find_if(presets, [&](const Preset& preset) {
        return (preset.variant == variant) && (preset.quirks == quirks);
    });
    return (it != presets.end()) ? &*it : nullptr;
}


auto find_preset(std::string_view id) noexcept -> const Preset* {
    const auto it = std::ranges::find(presets, id, &Preset::id);
    return (it != presets.end()) ? &*it : nullptr;
}


auto default_preset(Variant variant) noexcept -> const Preset& {
    const auto it = std::ranges::find(presets, variant, &Preset::variant);
    return (it != presets.end()) ? *it : presets[0];
}

} // namespace c8
