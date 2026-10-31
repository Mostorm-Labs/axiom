#pragma once

#include <array>
#include <cmath>
#include <cstdint>

namespace canvas::foundation {

enum class BackgroundMode : std::uint8_t { kPlain = 1, kDots = 2, kGrid = 3 };

// Renderer-neutral document value. No surface, storage or Runtime ownership.
struct DocumentBackground final {
    BackgroundMode mode = BackgroundMode::kPlain;
    std::array<float, 4> color{1, 1, 1, 1};
    std::array<float, 4> patternColor{0.7F, 0.75F, 0.8F, 0.55F};
    bool operator==(const DocumentBackground&) const = default;
};

[[nodiscard]] inline bool validDocumentBackground(const DocumentBackground& value) noexcept {
    if (value.mode != BackgroundMode::kPlain && value.mode != BackgroundMode::kDots &&
        value.mode != BackgroundMode::kGrid) return false;
    for (const auto& color : {value.color, value.patternColor})
        for (const float component : color)
            if (!std::isfinite(component) || component < 0 || component > 1) return false;
    return value.color[3] == 1;
}

} // namespace canvas::foundation
