#pragma once

#include "canvas/ink/vector_stroke_reference.hpp"

#include <cstdint>
#include <vector>

namespace canvas::ink {

struct BrushDab final {
    double x = 0.0;
    double y = 0.0;
    double size = 0.0;
    float rotation = 0.0F;
    float opacity = 0.0F;
    std::uint32_t materialRevision = 0;
    // Runtime material identity used by the shared renderer.  Zero preserves
    // the legacy solid-dab representation; package-backed materials use their
    // BrushMaterialMode value without exposing package semantics to platforms.
    std::uint32_t materialMode = 0;
    bool operator==(const BrushDab&) const = default;
};

// Immutable preview publication. A delta is replaceable and never contains
// confirmed samples or commit state.
struct BrushPreviewDelta final {
    std::uint64_t revision = 0;
    std::vector<reference::StrokeOutlinePoint> outline;
    std::vector<BrushDab> dabs;
};

}  // namespace canvas::ink
