#pragma once

#include "canvas/ink/brush_preview_delta.hpp"

#include <cstdint>
#include <vector>
#include <optional>
#include "canvas/runtime/ink_brush.hpp"

namespace canvas::ink {

struct BrushSample final {
    double x = 0.0;
    double y = 0.0;
    double pressure = 0.0;
    bool pressurePresent = false;
    std::uint64_t sequence = 0;
    std::uint64_t timestampNs = 0;
    bool pressureCapability = false;
    bool terminal = false;
};

// Confirmed-only transient handoff. Runtime Core, not BrushSession, owns
// construction of a semantic AddStroke operation.
struct BrushCommitIntent final {
    std::uint64_t session = 0;
    std::uint64_t revision = 0;
    std::uint64_t seed = 0;
    std::vector<BrushSample> confirmed;
    std::vector<reference::StrokeOutlinePoint> outline;
    std::vector<BrushDab> dabs;
    std::uint64_t dabDigest = 0;
    std::optional<foundation::InkBrushConfig> compactConfig;
    std::vector<foundation::InkKnot> knots;
};

}  // namespace canvas::ink
