#pragma once

#include "canvas/ink/brush_preview_delta.hpp"

#include <cstdint>
#include <vector>

namespace canvas::ink {

struct BrushSample final {
    double x = 0.0;
    double y = 0.0;
    double pressure = 0.0;
    bool pressurePresent = false;
    std::uint64_t sequence = 0;
};

// Confirmed-only transient handoff. Runtime Core, not BrushSession, owns
// construction of a semantic AddStroke operation.
struct BrushCommitIntent final {
    std::uint64_t session = 0;
    std::uint64_t revision = 0;
    std::uint64_t seed = 0;
    std::vector<BrushSample> confirmed;
    std::vector<reference::StrokeOutlinePoint> outline;
};

}  // namespace canvas::ink
