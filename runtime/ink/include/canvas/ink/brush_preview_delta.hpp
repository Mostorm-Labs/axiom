#pragma once

#include "canvas/ink/vector_stroke_reference.hpp"

#include <cstdint>
#include <vector>

namespace canvas::ink {

// Immutable preview publication. A delta is replaceable and never contains
// confirmed samples or commit state.
struct BrushPreviewDelta final {
    std::uint64_t revision = 0;
    std::vector<reference::StrokeOutlinePoint> outline;
};

}  // namespace canvas::ink
