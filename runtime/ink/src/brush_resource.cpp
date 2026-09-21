#include "canvas/ink/brush_resource.hpp"

#include <algorithm>
#include <cmath>

namespace canvas::ink {
namespace {

constexpr std::uint64_t kOffset = 1469598103934665603ULL;
constexpr std::uint64_t kPrime = 1099511628211ULL;

std::uint64_t hashBytes(ResourceId id, BrushResourceKind kind,
                        std::uint32_t width, std::uint32_t height,
                        const std::vector<std::uint8_t>& bytes) {
  std::uint64_t hash = kOffset;
  auto word = [&](std::uint64_t value) {
    for (unsigned shift = 0; shift != 64; shift += 8) {
      hash = (hash ^ ((value >> shift) & 0xffU)) * kPrime;
    }
  };
  word(id.value);
  word(static_cast<std::uint8_t>(kind));
  word(width);
  word(height);
  for (const auto byte : bytes) hash = (hash ^ byte) * kPrime;
  return hash == 0 ? 1 : hash;
}

BrushAlphaResource make(ResourceId id, std::uint32_t extent,
                        BrushResourceKind kind, BrushResourcePattern pattern) {
  BrushAlphaResource out{.id = id, .kind = kind, .width = extent, .height = extent,
                         .alpha = {}, .contentHash = 0};
  if (!id.valid() || extent == 0 || extent > 512) return {};
  out.alpha.resize(static_cast<std::size_t>(extent) * extent);
  for (std::uint32_t y = 0; y < extent; ++y) {
    for (std::uint32_t x = 0; x < extent; ++x) {
      const float fx = (2.0F * (static_cast<float>(x) + 0.5F) / extent) - 1.0F;
      const float fy = (2.0F * (static_cast<float>(y) + 0.5F) / extent) - 1.0F;
      const float radius = std::sqrt(fx * fx + fy * fy);
      float value = std::clamp((1.0F - radius) * 3.0F, 0.0F, 1.0F);
      if (pattern == BrushResourcePattern::kBristle) {
        const float stripes = 0.55F + 0.45F * std::abs(std::sin(fx * 31.0F));
        value *= stripes;
      } else if (pattern == BrushResourcePattern::kPaper) {
        const auto cell = (x * 37U + y * 61U + x * y * 7U) & 31U;
        value = 0.55F + static_cast<float>(cell) / 70.0F;
      } else if (pattern == BrushResourcePattern::kSpeckle) {
        const auto cell = (x * 73U + y * 109U + x * y * 13U) & 63U;
        value *= cell < 18U ? 0.15F : 1.0F;
      } else if (pattern == BrushResourcePattern::kFlat) {
        value = std::abs(fx) <= 0.9F && std::abs(fy) <= 0.45F ? 1.0F : 0.0F;
      }
      out.alpha[static_cast<std::size_t>(y) * extent + x] =
          static_cast<std::uint8_t>(std::lround(std::clamp(value, 0.0F, 1.0F) * 255.0F));
    }
  }
  out.contentHash = hashBytes(id, kind, extent, extent, out.alpha);
  return out;
}

}  // namespace

BrushAlphaResource makeProceduralShape(ResourceId id, std::uint32_t extent,
                                       BrushResourcePattern pattern) {
  return make(id, extent, BrushResourceKind::kShape, pattern);
}

BrushAlphaResource makeProceduralGrain(ResourceId id, std::uint32_t extent,
                                       BrushResourcePattern pattern) {
  return make(id, extent, BrushResourceKind::kGrain, pattern);
}

}  // namespace canvas::ink
