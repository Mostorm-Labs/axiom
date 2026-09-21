#include "canvas/ink/reference_brush_catalog.hpp"

#include <sstream>

namespace canvas::ink {
namespace {
BrushDefinition definition(std::uint64_t id, BrushFamily family, float size,
                           float opacity, float spacing, ResourceId shape,
                           ResourceId grain) {
  return {.definitionId = id, .version = 2, .family = family,
          .nominalSize = size, .opacity = opacity, .spacing = spacing,
          .pressureSizeInfluence = 0.55F, .pressureOpacityInfluence = 0.2F,
          .tiltSizeInfluence = 0.1F, .tiltRotationInfluence = 0.1F,
          .shapeResource = shape, .grainResource = grain};
}
BrushResourcePattern shapePattern(ReferenceBrushId id) {
  return id == ReferenceBrushId::kDryChalk ? BrushResourcePattern::kBristle
      : id == ReferenceBrushId::kDecorativeBroad ? BrushResourcePattern::kFlat
      : BrushResourcePattern::kRound;
}
BrushResourcePattern grainPattern(ReferenceBrushId id) {
  return id == ReferenceBrushId::kSoftAirbrush ? BrushResourcePattern::kSpeckle
      : id == ReferenceBrushId::kDryChalk ? BrushResourcePattern::kPaper
      : id == ReferenceBrushId::kDecorativeBroad ? BrushResourcePattern::kBristle
      : id == ReferenceBrushId::kPressureMarker ? BrushResourcePattern::kPaper
      : BrushResourcePattern::kFlat;
}
}

ReferenceBrushCatalog makeReferenceBrushCatalog() {
  ReferenceBrushCatalog out;
  const auto add = [&](ReferenceBrushId id, const char* name, BrushDefinition value) {
    out.presets.push_back({id, name, value});
    out.resources.add(makeProceduralShape(value.shapeResource, 16, shapePattern(id)));
    out.resources.add(makeProceduralGrain(value.grainResource, 16, grainPattern(id)));
  };
  add(ReferenceBrushId::kFineInk, "RB-01 Fine Ink", definition(501, BrushFamily::kPencil, 6.0F, 0.95F, 0.16F, {5011}, {5012}));
  add(ReferenceBrushId::kPressureMarker, "RB-02 Pressure Marker", definition(502, BrushFamily::kMarker, 14.0F, 0.82F, 0.24F, {5021}, {5022}));
  add(ReferenceBrushId::kDryChalk, "RB-03 Dry Chalk", definition(503, BrushFamily::kChalk, 12.0F, 0.72F, 0.3F, {5031}, {5032}));
  add(ReferenceBrushId::kSoftAirbrush, "RB-04 Soft Airbrush", definition(504, BrushFamily::kWaterColorLite, 22.0F, 0.34F, 0.18F, {5041}, {5042}));
  add(ReferenceBrushId::kDecorativeBroad, "RB-05 Decorative Broad", definition(505, BrushFamily::kMarker, 20.0F, 0.68F, 0.32F, {5051}, {5052}));
  return out;
}

const ReferenceBrushPreset* ReferenceBrushCatalog::find(ReferenceBrushId id) const noexcept {
  for (const auto& preset : presets) if (preset.id == id) return &preset;
  return nullptr;
}

BrushCompileResult compileReferenceBrush(const ReferenceBrushPreset& preset,
                                         const ResourceCatalog& resources) {
  const auto compiled = BrushCompiler{}.compile(
      preset.definition, {.pressure = true, .tilt = true, .shapeResource = true,
                          .grainResource = true, .temporalTransient = true});
  if (!compiled) return compiled;
  if (resources.resource(preset.definition.shapeResource) == nullptr ||
      resources.resource(preset.definition.grainResource) == nullptr ||
      !resources.renderResource(preset.definition.shapeResource,
                                preset.definition.grainResource)) {
    return {nullptr, BrushCompileError::kUnsupportedCapability};
  }
  return compiled;
}

std::string referenceBrushCatalogManifestJson() {
  const auto catalog = makeReferenceBrushCatalog();
  std::ostringstream out;
  out << "{\"schema_version\":\"0.1\",\"brush_count\":5,\"brushes\":[";
  for (std::size_t i = 0; i < catalog.presets.size(); ++i) {
    const auto& preset = catalog.presets[i];
    const auto program = compileReferenceBrush(preset, catalog.resources);
    if (i != 0) out << ',';
    out << "{\"id\":" << static_cast<unsigned>(preset.id)
        << ",\"name\":\"" << preset.name << "\",\"program_identity\":"
        << (program ? program.program->identity() : 0) << '}';
  }
  out << "]}";
  return out.str();
}
}  // namespace canvas::ink
