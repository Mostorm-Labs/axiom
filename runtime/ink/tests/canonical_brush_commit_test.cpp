#include "canvas/ink/canonical_brush_commit.hpp"

#include <array>
#include <cassert>
#include <variant>

namespace ink = canvas::ink;
namespace semantic = canvas::semantic;

namespace {

ink::BrushDefinition definition(ink::BrushFamily family) {
  ink::BrushDefinition value;
  value.definitionId = static_cast<std::uint64_t>(family);
  value.family = family;
  value.nominalSize = 6;
  value.opacity = 0.8F;
  value.spacing = 0.2F;
  value.pressureSizeInfluence = 0.5F;
  value.pressureOpacityInfluence = 0.25F;
  if (family == ink::BrushFamily::kPencil || family == ink::BrushFamily::kChalk ||
      family == ink::BrushFamily::kMarker ||
      family == ink::BrushFamily::kWaterColorLite) {
    value.shapeResource = {100 + static_cast<std::uint64_t>(family)};
    value.grainResource = {200 + static_cast<std::uint64_t>(family)};
  }
  return value;
}

void verify(ink::BrushFamily family) {
  const auto def = definition(family);
  const auto compiled = ink::BrushCompiler{}.compile(
      def, {.pressure = true, .tilt = true, .shapeResource = true,
            .grainResource = true, .temporalTransient = true});
  assert(compiled);
  ink::ResourceCatalog catalog;
  if (def.shapeResource.valid()) catalog.add(def.shapeResource, ink::BrushResourceKind::kShape);
  if (def.grainResource.valid()) catalog.add(def.grainResource, ink::BrushResourceKind::kGrain);
  ink::BrushRuntime runtime(catalog);
  assert(runtime.begin({1}, *compiled.program, 55));
  const std::array samples{
      ink::BrushInputSample{1, 2, 0.5F, 0, 0, 1},
      ink::BrushInputSample{3, 4, 1.0F, 0, 0, 2}};
  assert(runtime.append({1}, samples));
  const auto artifact = runtime.finish({1});
  assert(artifact);
  const auto canonical = ink::toCanonicalStroke(*compiled.program, artifact.commit);
  if (family == ink::BrushFamily::kLaser) {
    assert(!canonical);
    return;
  }
  assert(canonical);
  assert(canonical->deterministic_seed == 55);
  if (compiled.program->representation() == ink::BrushRepresentation::kDab) {
    assert(canonical->brush.brush_family_id == 3);
    assert(canonical->brush.texture_resource_id.has_value());
    assert(std::holds_alternative<semantic::DabStrokeData>(canonical->data));
  } else {
    assert(canonical->brush.brush_family_id ==
           (family == ink::BrushFamily::kHighlighter ? 2U : 1U));
    assert(std::holds_alternative<semantic::VectorStrokeData>(canonical->data));
  }
}

}  // namespace

int main() {
  for (const auto family : {ink::BrushFamily::kPen, ink::BrushFamily::kPencil,
                            ink::BrushFamily::kChalk, ink::BrushFamily::kMarker,
                            ink::BrushFamily::kWaterColorLite,
                            ink::BrushFamily::kHighlighter,
                            ink::BrushFamily::kLaser}) {
    verify(family);
  }
  return 0;
}
