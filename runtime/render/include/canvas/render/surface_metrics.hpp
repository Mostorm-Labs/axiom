#pragma once

#include <cstdint>

namespace canvas::render {

// Renderer-neutral surface measurements captured for one frame.
struct SurfaceMetrics final {
    const float logicalWidth = 0.0F;
    const float logicalHeight = 0.0F;
    const std::uint32_t physicalWidth = 0;
    const std::uint32_t physicalHeight = 0;
    const float devicePixelRatio = 1.0F;
    const float displayScale = 1.0F;

    bool operator==(const SurfaceMetrics&) const = default;
};

} // namespace canvas::render
