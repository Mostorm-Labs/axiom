#pragma once

#include "canvas/ink/programmable_brush.hpp"

#include <cstdint>
#include <span>
#include <vector>

namespace canvas::ink {
struct VectorStrokeParameters final {
  float size = 1.0F;
  float thinning = 0.0F;
  float smoothing = 0.0F;
  float startTaper = 0.35F;
  float endTaper = 0.35F;
};
struct VectorStrokeVertex final {
  float x = 0.0F;
  float y = 0.0F;
  float width = 0.0F;
  float opacity = 1.0F;
  bool operator==(const VectorStrokeVertex&) const = default;
};
struct VectorStrokeGeometry final {
  std::vector<VectorStrokeVertex> vertices;
  std::vector<std::uint32_t> indices;
  std::vector<BrushInputSample> sourceSamples;
  std::uint64_t geometryDigest = 0;
  std::size_t stableVertexCount = 0;
};
[[nodiscard]] VectorStrokeGeometry generateVectorStroke(
    std::span<const BrushInputSample> samples, VectorStrokeParameters parameters);
[[nodiscard]] VectorStrokeGeometry appendVectorStroke(
    const VectorStrokeGeometry& previous, std::span<const BrushInputSample> samples,
    VectorStrokeParameters parameters);
}  // namespace canvas::ink
