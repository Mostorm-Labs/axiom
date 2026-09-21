#include "reference_brushes.hpp"

#include <array>
#include <cassert>

int main() {
  using namespace canvas::brush_lab;
  const auto set = makeReferenceBrushSet();
  assert(set.presets.size() == 5);
  for (const auto& preset : set.presets) {
    assert(preset.definition.version == 2);
    assert(preset.definition.shapeResource.valid());
    assert(preset.definition.grainResource.valid());
    const auto compiled = compileReferenceBrush(preset, set.resources);
    assert(compiled);
    assert(compiled.program->representation() ==
           canvas::ink::BrushRepresentation::kDab);
  }

  const auto* rb03 = set.find(ReferenceBrushId::kDryChalk);
  assert(rb03 != nullptr);
  const auto original = compileReferenceBrush(*rb03, set.resources);
  const auto editedPreset = editDryChalk(*rb03, {.nominalSize = 19.0F,
                                                  .spacing = 0.34F,
                                                  .opacity = 0.62F});
  const auto edited = compileReferenceBrush(editedPreset, set.resources);
  assert(original && edited);
  assert(original.program->identity() != edited.program->identity());
  const auto restored = compileReferenceBrush(*rb03, set.resources);
  assert(restored.program->identity() == original.program->identity());
}
