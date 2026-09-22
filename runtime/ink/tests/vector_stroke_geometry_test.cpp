#include "canvas/ink/vector_stroke_geometry.hpp"

#include <array>
#include <cassert>
#include <cmath>

using namespace canvas::ink;

int main() {
  const std::array samples{
      BrushInputSample{0.0F, 0.0F, 0.2F, 0.0F, 0.0F, 1U},
      BrushInputSample{20.0F, 3.0F, 0.7F, 0.0F, 0.0F, 2U},
      BrushInputSample{40.0F, 0.0F, 1.0F, 0.0F, 0.0F, 3U}};
  VectorStrokeParameters parameters{};
  parameters.size = 10.0F;
  parameters.thinning = 0.5F;
  parameters.smoothing = 0.25F;
  const auto oneShot = generateVectorStroke(samples, parameters);
  assert(oneShot.vertices.size() > samples.size());
  assert(oneShot.indices.size() >= 3U);
  assert(oneShot.geometryDigest != 0U);
  assert(oneShot.vertices.front().width < oneShot.vertices[oneShot.vertices.size() / 2U].width);
  const auto first = generateVectorStroke(std::span(samples).first(2), parameters);
  const auto second = appendVectorStroke(first, std::span(samples).subspan(2), parameters);
  assert(second.geometryDigest == oneShot.geometryDigest);

  const std::array continuation{
      BrushInputSample{60.0F, 4.0F, 0.6F, 0.0F, 0.0F, 4U},
      BrushInputSample{80.0F, 0.0F, 0.3F, 0.0F, 0.0F, 5U}};
  const auto extended = appendVectorStroke(oneShot, continuation, parameters);
  assert(extended.stableVertexCount >= oneShot.stableVertexCount);
  for (std::size_t index = 0; index < oneShot.stableVertexCount; ++index) {
    assert(std::fabs(extended.vertices[index].x - oneShot.vertices[index].x) < 0.0001F);
    assert(std::fabs(extended.vertices[index].y - oneShot.vertices[index].y) < 0.0001F);
    assert(std::fabs(extended.vertices[index].width - oneShot.vertices[index].width) < 0.0001F);
  }

  BrushDefinition v2;
  v2.definitionId = 4501U;
  v2.version = 2U;
  v2.family = BrushFamily::kPen;
  v2.nominalSize = parameters.size;
  v2.opacity = 1.0F;
  v2.spacing = 0.1F;
  v2.pressureSizeInfluence = parameters.thinning;
  v2.smoothing = parameters.smoothing;
  const auto compiled = BrushCompiler{}.compile(
      v2, {.pressure = true, .tilt = true, .shapeResource = true,
           .grainResource = true, .temporalTransient = true});
  assert(compiled);
  ResourceCatalog resources;
  BrushRuntime runtime(resources);
  assert(runtime.begin({1U}, *compiled.program, 77U));
  assert(runtime.append({1U}, samples));
  const auto artifact = runtime.finish({1U});
  assert(artifact);
  assert(artifact.commit.digest == oneShot.geometryDigest);
  assert(artifact.commit.primitives.size() == oneShot.vertices.size());

  const std::array taperSamples{
      BrushInputSample{0, 0, 0.5F, 0, 0, 1}, BrushInputSample{10, 0, 0.5F, 0, 0, 2},
      BrushInputSample{20, 0, 0.5F, 0, 0, 3}, BrushInputSample{30, 0, 0.5F, 0, 0, 4},
      BrushInputSample{40, 0, 0.5F, 0, 0, 5}, BrushInputSample{50, 0, 0.5F, 0, 0, 6},
      BrushInputSample{60, 0, 0.5F, 0, 0, 7}, BrushInputSample{70, 0, 0.5F, 0, 0, 8},
      BrushInputSample{80, 0, 0.5F, 0, 0, 9}};
  const auto tapered = generateVectorStroke(
      taperSamples, {.size = 12.0F, .thinning = 0.0F, .smoothing = 0.0F});
  assert(tapered.vertices[0].width < tapered.vertices[2].width);
  assert(tapered.vertices[2].width < tapered.vertices[4].width);
  assert(tapered.vertices[tapered.vertices.size() - 1U].width <
         tapered.vertices[tapered.vertices.size() - 3U].width);
  assert(tapered.vertices[tapered.vertices.size() - 3U].width <
         tapered.vertices[tapered.vertices.size() - 5U].width);

  const auto fast = generateVectorStroke(
      taperSamples, {.size = 12.0F, .thinning = 0.65F, .smoothing = 0.0F,
                      .simulatePressure = true, .startTaper = 0.0F,
                      .endTaper = 0.0F});
  const std::array slowSamples{
      BrushInputSample{0, 0, 0.5F, 0, 0, 1}, BrushInputSample{1, 0, 0.5F, 0, 0, 2},
      BrushInputSample{2, 0, 0.5F, 0, 0, 3}, BrushInputSample{3, 0, 0.5F, 0, 0, 4},
      BrushInputSample{4, 0, 0.5F, 0, 0, 5}, BrushInputSample{5, 0, 0.5F, 0, 0, 6},
      BrushInputSample{6, 0, 0.5F, 0, 0, 7}, BrushInputSample{7, 0, 0.5F, 0, 0, 8},
      BrushInputSample{8, 0, 0.5F, 0, 0, 9}};
  const auto slow = generateVectorStroke(
      slowSamples, {.size = 12.0F, .thinning = 0.65F, .smoothing = 0.0F,
                     .simulatePressure = true, .startTaper = 0.0F,
                     .endTaper = 0.0F});
  assert(fast.vertices[8].width < slow.vertices[8].width);
}
