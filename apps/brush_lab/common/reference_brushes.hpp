#pragma once

#include "canvas/ink/brush_resource.hpp"

#include <vector>

namespace canvas::brush_lab {

enum class ReferenceBrushId : std::uint8_t {
  kFineInk = 1,
  kPressureMarker = 2,
  kDryChalk = 3,
  kSoftAirbrush = 4,
  kDecorativeBroad = 5,
};

struct ReferenceBrushPreset final {
  ReferenceBrushId id{};
  const char* name = nullptr;
  ink::BrushDefinition definition{};
};

struct ReferenceBrushSet final {
  std::vector<ReferenceBrushPreset> presets;
  ink::ResourceCatalog resources;
  [[nodiscard]] const ReferenceBrushPreset* find(ReferenceBrushId id) const noexcept;
};

struct DryChalkEdit final {
  float nominalSize = 0.0F;
  float spacing = 0.0F;
  float opacity = 0.0F;
};

[[nodiscard]] ReferenceBrushSet makeReferenceBrushSet();
[[nodiscard]] ink::BrushCompileResult compileReferenceBrush(
    const ReferenceBrushPreset& preset, const ink::ResourceCatalog& resources);
[[nodiscard]] ReferenceBrushPreset editDryChalk(const ReferenceBrushPreset& preset,
                                                DryChalkEdit edit);

}  // namespace canvas::brush_lab
