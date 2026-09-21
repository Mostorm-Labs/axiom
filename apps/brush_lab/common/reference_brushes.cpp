#include "reference_brushes.hpp"

namespace canvas::brush_lab {
namespace {

ink::BrushDefinition definition(std::uint64_t id, ink::BrushFamily family,
                                float size, float opacity, float spacing,
                                ink::ResourceId shape, ink::ResourceId grain) {
  return {.definitionId = id,
          .version = 2,
          .family = family,
          .nominalSize = size,
          .opacity = opacity,
          .spacing = spacing,
          .pressureSizeInfluence = 0.55F,
          .pressureOpacityInfluence = 0.2F,
          .tiltSizeInfluence = 0.1F,
          .tiltRotationInfluence = 0.1F,
          .shapeResource = shape,
          .grainResource = grain};
}

}  // namespace

ReferenceBrushSet makeReferenceBrushSet() {
  ReferenceBrushSet out;
  const auto add = [&](ReferenceBrushId id, const char* name,
                       ink::BrushDefinition value) {
    out.presets.push_back({id, name, value});
    out.resources.add(ink::makeProceduralShape(value.shapeResource, 16,
                                                id == ReferenceBrushId::kDryChalk
                                                    ? ink::BrushResourcePattern::kBristle
                                                    : ink::BrushResourcePattern::kRound));
    out.resources.add(ink::makeProceduralGrain(value.grainResource, 16,
                                               id == ReferenceBrushId::kSoftAirbrush
                                                   ? ink::BrushResourcePattern::kSpeckle
                                                   : ink::BrushResourcePattern::kPaper));
  };
  add(ReferenceBrushId::kFineInk, "RB-01 Fine Ink",
      definition(501, ink::BrushFamily::kPencil, 6.0F, 0.95F, 0.16F, {5011}, {5012}));
  add(ReferenceBrushId::kPressureMarker, "RB-02 Pressure Marker",
      definition(502, ink::BrushFamily::kMarker, 14.0F, 0.82F, 0.24F, {5021}, {5022}));
  add(ReferenceBrushId::kDryChalk, "RB-03 Dry Chalk",
      definition(503, ink::BrushFamily::kChalk, 12.0F, 0.72F, 0.3F, {5031}, {5032}));
  add(ReferenceBrushId::kSoftAirbrush, "RB-04 Soft Airbrush",
      definition(504, ink::BrushFamily::kWaterColorLite, 22.0F, 0.34F, 0.18F, {5041}, {5042}));
  add(ReferenceBrushId::kDecorativeBroad, "RB-05 Decorative Broad",
      definition(505, ink::BrushFamily::kMarker, 20.0F, 0.68F, 0.32F, {5051}, {5052}));
  return out;
}

const ReferenceBrushPreset* ReferenceBrushSet::find(ReferenceBrushId id) const noexcept {
  for (const auto& preset : presets) if (preset.id == id) return &preset;
  return nullptr;
}

ink::BrushCompileResult compileReferenceBrush(
    const ReferenceBrushPreset& preset, const ink::ResourceCatalog& resources) {
  const auto compiled = ink::BrushCompiler{}.compile(
      preset.definition, {.pressure = true, .tilt = true, .shapeResource = true,
                          .grainResource = true, .temporalTransient = true});
  if (!compiled) return compiled;
  const auto* shape = resources.resource(preset.definition.shapeResource);
  const auto* grain = resources.resource(preset.definition.grainResource);
  if (shape == nullptr || grain == nullptr || !resources.renderResource(
          preset.definition.shapeResource, preset.definition.grainResource)) {
    return {nullptr, ink::BrushCompileError::kUnsupportedCapability};
  }
  return compiled;
}

ReferenceBrushPreset editDryChalk(const ReferenceBrushPreset& preset,
                                  DryChalkEdit edit) {
  auto out = preset;
  out.definition.nominalSize = edit.nominalSize;
  out.definition.spacing = edit.spacing;
  out.definition.opacity = edit.opacity;
  return out;
}

}  // namespace canvas::brush_lab
