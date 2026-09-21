#pragma once

#include "canvas/ink/programmable_brush.hpp"

#include <cstdint>
#include <vector>

namespace canvas::ink {

enum class BrushResourcePattern : std::uint8_t {
  kRound = 1,
  kBristle = 2,
  kPaper = 3,
  kSpeckle = 4,
  kFlat = 5,
};

struct BrushAlphaResource final {
  ResourceId id{};
  BrushResourceKind kind = BrushResourceKind::kShape;
  std::uint32_t width = 0;
  std::uint32_t height = 0;
  std::vector<std::uint8_t> alpha;
  std::uint64_t contentHash = 0;
};

struct BrushRenderResource final {
  ResourceId id{};
  ResourceId shape{};
  ResourceId grain{};
  std::uint32_t width = 0;
  std::uint32_t height = 0;
  std::vector<std::uint8_t> alpha;
  std::uint64_t contentHash = 0;
};

[[nodiscard]] BrushAlphaResource makeProceduralShape(
    ResourceId id, std::uint32_t extent, BrushResourcePattern pattern);
[[nodiscard]] BrushAlphaResource makeProceduralGrain(
    ResourceId id, std::uint32_t extent, BrushResourcePattern pattern);

}  // namespace canvas::ink
