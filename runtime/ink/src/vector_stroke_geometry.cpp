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
float taper(float t, float amount) noexcept {
  if (amount <= 0.0F) return 1.0F;
  return t < amount ? std::clamp(t / amount, 0.0F, 1.0F)
       : t > 1.0F - amount ? std::clamp((1.0F - t) / amount, 0.0F, 1.0F) : 1.0F;
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
  for (std::size_t i = 0; i < samples.size(); ++i) {
    const auto& current = samples[i];
    if (i == 0U || parameters.smoothing == 0.0F) smoothed.push_back(current);
    else { const auto& previous = smoothed.back(); const float amount = parameters.smoothing * 0.5F;
      smoothed.push_back({previous.x + (current.x - previous.x) * amount,
                          previous.y + (current.y - previous.y) * amount, current.pressure,
                          current.tiltX, current.tiltY, current.sequence}); }
  }
  const std::size_t count = smoothed.size();
  out.vertices.reserve(count * 2U);
  for (std::size_t i = 0; i < count; ++i) {
    const auto& point = smoothed[i]; const auto& before = smoothed[i == 0U ? i : i - 1U];
    const auto& after = smoothed[i + 1U < count ? i + 1U : i];
    float dx = after.x - before.x; float dy = after.y - before.y; const float length = std::hypot(dx, dy);
    if (length < 0.0001F) { dx = 1.0F; dy = 0.0F; } else { dx /= length; dy /= length; }
    const float pressure = std::clamp(point.pressure, 0.0F, 1.0F);
    const float t = count <= 1U ? 0.0F : static_cast<float>(i) / static_cast<float>(count - 1U);
    const float width = std::max(0.01F, parameters.size *
        (1.0F + parameters.thinning * (pressure * 2.0F - 1.0F)) *
        taper(t, i == 0U ? parameters.startTaper : i + 1U == count ? parameters.endTaper : 0.0F));
    const float nx = -dy * width * 0.5F; const float ny = dx * width * 0.5F;
    out.vertices.push_back({point.x + nx, point.y + ny, width, pressure});
    out.vertices.push_back({point.x - nx, point.y - ny, width, pressure});
    if (i > 0U) { const auto base = static_cast<std::uint32_t>(i * 2U); const auto prev = base - 2U;
      out.indices.insert(out.indices.end(), {prev, prev + 1U, base, base, prev + 1U, base + 1U}); }
  }
  out.stableVertexCount = out.vertices.size() > 4U ? out.vertices.size() - 4U : 0U;
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
