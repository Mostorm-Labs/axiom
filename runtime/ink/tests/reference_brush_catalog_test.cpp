#include "canvas/ink/reference_brush_catalog.hpp"

#include <cassert>
#include <string_view>

using namespace canvas::ink;

int main() {
  const auto catalog = makeReferenceBrushCatalog();
  assert(catalog.presets.size() == 5U);
  assert(catalog.find(ReferenceBrushId::kFineInk)->name == std::string_view{"RB-01 Fine Ink"});
  assert(catalog.find(ReferenceBrushId::kPressureMarker)->name == std::string_view{"RB-02 Pressure Marker"});
  assert(catalog.find(ReferenceBrushId::kDryChalk)->name == std::string_view{"RB-03 Dry Chalk"});
  assert(catalog.find(ReferenceBrushId::kSoftAirbrush)->name == std::string_view{"RB-04 Soft Airbrush"});
  assert(catalog.find(ReferenceBrushId::kDecorativeBroad)->name == std::string_view{"RB-05 Decorative Broad"});
  for (const auto& preset : catalog.presets) {
    assert(preset.definition.version == 2U);
    assert(preset.definition.shapeResource.valid());
    assert(preset.definition.grainResource.valid());
    assert(catalog.resources.resource(preset.definition.shapeResource) != nullptr);
    assert(catalog.resources.resource(preset.definition.grainResource) != nullptr);
    assert(catalog.resources.renderResource(preset.definition.shapeResource,
                                            preset.definition.grainResource) != nullptr);
    assert(compileReferenceBrush(preset, catalog.resources));
  }
  const auto manifest = referenceBrushCatalogManifestJson();
  assert(manifest.find("RB-01 Fine Ink") != std::string::npos);
  assert(manifest.find("RB-05 Decorative Broad") != std::string::npos);
}
