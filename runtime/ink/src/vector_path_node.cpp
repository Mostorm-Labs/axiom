#include "canvas/ink/vector_path_node.hpp"

namespace canvas::ink {

VectorPathResult VectorPathNode::evaluate(
    std::span<const reference::VectorStrokeInput> samples, bool last) const {
  reference::StrokeOptions options;
  const auto& v = state_.package.vector;
  options.size = v.size;
  options.thinning = v.thinning;
  options.smoothing = v.smoothing;
  options.streamline = v.streamline;
  options.simulate_pressure = v.pressureSource == BrushPressureSource::kSimulated;
  options.start_cap = v.startCap;
  options.end_cap = v.endCap;
  if (v.startTaper > 0.0) {
    options.start_taper = v.startTaper;
    options.start_taper_enabled = true;
  }
  if (v.endTaper > 0.0) {
    options.end_taper = v.endTaper;
    options.end_taper_enabled = true;
  }
  options.last = last;
  auto points = reference::getStrokePoints(samples, options);
  auto outline = reference::getStrokeOutlinePoints(points, options);
  return {std::move(points), std::move(outline), {}};
}

}  // namespace canvas::ink
