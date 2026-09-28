#pragma once

#include "canvas/ink/brush_preview_delta.hpp"
#include "canvas/ink/resolved_brush_state.hpp"
#include "canvas/ink/vector_stroke_reference.hpp"

#include <cstdint>
#include <span>
#include <vector>

namespace canvas::ink {

// The dab representation owns its path finalization independently of the
// outline preview. Both preview and seal call this evaluator with the same
// normalized inputs; predicted inputs are supplied only by preview callers.
class ChalkDabEvaluator final {
  public:
    [[nodiscard]] static std::vector<BrushDab> evaluate(
        const ResolvedBrushState& state,
        std::span<const reference::VectorStrokeInput> samples);
    [[nodiscard]] static std::uint64_t digest(
        std::span<const BrushDab> dabs, std::uint64_t seed);
};

} // namespace canvas::ink
