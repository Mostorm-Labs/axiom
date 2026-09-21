#pragma once

#include "canvas/ink/brush_resource.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace canvas::ink {

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
  BrushDefinition definition{};
};

struct ReferenceBrushCatalog final {
  std::vector<ReferenceBrushPreset> presets;
  ResourceCatalog resources;
  [[nodiscard]] const ReferenceBrushPreset* find(ReferenceBrushId id) const noexcept;
};

[[nodiscard]] ReferenceBrushCatalog makeReferenceBrushCatalog();
[[nodiscard]] std::string referenceBrushCatalogManifestJson();
[[nodiscard]] BrushCompileResult compileReferenceBrush(
    const ReferenceBrushPreset& preset, const ResourceCatalog& resources);

}  // namespace canvas::ink
