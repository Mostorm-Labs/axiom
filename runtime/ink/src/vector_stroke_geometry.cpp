#include "canvas/ink/vector_stroke_geometry.hpp"

#include <algorithm>
#include <bit>
#include <cmath>

namespace canvas::ink {
namespace {
constexpr std::uint64_t kOffset = 1469598103934665603ULL;
constexpr std::uint64_t kPrime = 1099511628211ULL;
void hashWord(std::uint64_t& hash, std::uint64_t value) noexcept {
  for (unsigned shift = 0; shift < 64; shift += 8) { hash ^= (value >> shift) & 0xffU; hash *= kPrime; }
}
void hashFloat(std::uint64_t& hash, float value) noexcept { hashWord(hash, std::bit_cast<std::uint32_t>(value)); }
float startTaperEase(float t) noexcept {
  t = std::clamp(t, 0.0F, 1.0F);
  return t * (2.0F - t);
}
float endTaperEase(float t) noexcept {
  t = std::clamp(t, 0.0F, 1.0F) - 1.0F;
  return t * t * t + 1.0F;
}
}
VectorStrokeGeometry generateVectorStroke(std::span<const BrushInputSample> samples,
                                          VectorStrokeParameters parameters) {
  VectorStrokeGeometry out;
  if (samples.empty() || !std::isfinite(parameters.size) || parameters.size <= 0.0F) return out;
  parameters.thinning = std::clamp(parameters.thinning, -1.0F, 1.0F);
  parameters.smoothing = std::clamp(parameters.smoothing, 0.0F, 1.0F);
  out.sourceSamples.assign(samples.begin(), samples.end());
  std::vector<BrushInputSample> smoothed;
  smoothed.reserve(samples.size());
  const float streamline = 0.15F + (1.0F - parameters.smoothing) * 0.85F;
  for (std::size_t i = 0; i < samples.size(); ++i) {
    const auto& current = samples[i];
    if (i == 0U || parameters.smoothing == 0.0F) smoothed.push_back(current);
    else { const auto& previous = smoothed.back();
      smoothed.push_back({previous.x + (current.x - previous.x) * streamline,
                          previous.y + (current.y - previous.y) * streamline, current.pressure,
                          current.tiltX, current.tiltY, current.sequence}); }
  }
  const std::size_t count = smoothed.size();
  std::vector<float> runningLengths(count, 0.0F);
  for (std::size_t i = 1U; i < count; ++i) {
    runningLengths[i] = runningLengths[i - 1U] +
        std::hypot(smoothed[i].x - smoothed[i - 1U].x,
                   smoothed[i].y - smoothed[i - 1U].y);
  }
  const float totalLength = runningLengths.empty() ? 0.0F : runningLengths.back();
  // RB-V01 stores taper as fixed version semantics. Convert the normalized
  // interpreter constant into an absolute brush-size-relative distance so
  // extending an active stroke cannot rewrite its already stable start.
  const float startTaperDistance = parameters.size * 4.0F *
      std::clamp(parameters.startTaper, 0.0F, 1.0F);
  const float endTaperDistance = parameters.size * 4.0F *
      std::clamp(parameters.endTaper, 0.0F, 1.0F);
  out.vertices.reserve(count * 2U);
  float simulatedPressure = 0.25F;
  for (std::size_t i = 0; i < count; ++i) {
    const auto& point = smoothed[i]; const auto& before = smoothed[i == 0U ? i : i - 1U];
    const auto& after = smoothed[i + 1U < count ? i + 1U : i];
    float dx = after.x - before.x; float dy = after.y - before.y; const float length = std::hypot(dx, dy);
    if (length < 0.0001F) { dx = 1.0F; dy = 0.0F; } else { dx /= length; dy /= length; }
    float pressure = std::clamp(point.pressure, 0.0F, 1.0F);
    if (parameters.simulatePressure) {
      const float distance = i == 0U ? 0.0F : runningLengths[i] - runningLengths[i - 1U];
      const float speed = (std::min)(1.0F, distance / parameters.size);
      const float target = (std::min)(1.0F, 1.0F - speed);
      simulatedPressure = (std::min)(
          1.0F, simulatedPressure +
                    (target - simulatedPressure) * (speed * 0.275F));
      pressure = simulatedPressure;
    }
    const float fromStart = runningLengths[i];
    const float fromEnd = totalLength - runningLengths[i];
    const float startStrength = startTaperDistance > 0.0F && fromStart < startTaperDistance
                                    ? startTaperEase(fromStart / startTaperDistance) : 1.0F;
    const float endStrength = endTaperDistance > 0.0F && fromEnd < endTaperDistance
                                  ? endTaperEase(fromEnd / endTaperDistance) : 1.0F;
    const float taperFactor = (std::min)(startStrength, endStrength);
    const float width = std::max(0.01F, parameters.size *
        (1.0F + parameters.thinning * (pressure * 2.0F - 1.0F)) * taperFactor);
    const float nx = -dy * width * 0.5F; const float ny = dx * width * 0.5F;
    out.vertices.push_back({point.x + nx, point.y + ny, width, pressure});
    out.vertices.push_back({point.x - nx, point.y - ny, width, pressure});
    if (i > 0U) { const auto base = static_cast<std::uint32_t>(i * 2U); const auto prev = base - 2U;
      out.indices.insert(out.indices.end(), {prev, prev + 1U, base, base, prev + 1U, base + 1U}); }
  }
  std::size_t stablePointCount = 0U;
  const float replaceableTailStart = totalLength - endTaperDistance;
  while (stablePointCount < runningLengths.size() &&
         runningLengths[stablePointCount] < replaceableTailStart) {
    ++stablePointCount;
  }
  out.stableVertexCount = stablePointCount * 2U;
  std::uint64_t digest = kOffset;
  for (const auto& vertex : out.vertices) { hashFloat(digest, vertex.x); hashFloat(digest, vertex.y); hashFloat(digest, vertex.width); hashFloat(digest, vertex.opacity); }
  for (const auto index : out.indices) hashWord(digest, index);
  out.geometryDigest = digest; return out;
}
VectorStrokeGeometry appendVectorStroke(const VectorStrokeGeometry& previous,
                                        std::span<const BrushInputSample> samples,
                                        VectorStrokeParameters parameters) {
  std::vector<BrushInputSample> combined = previous.sourceSamples;
  combined.insert(combined.end(), samples.begin(), samples.end());
  return generateVectorStroke(combined, parameters);
}
}  // namespace canvas::ink
