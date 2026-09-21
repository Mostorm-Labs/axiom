#include "reference_brushes.hpp"

#include <cassert>
#include <string>
#include <array>
#include <set>

int main() {
  using namespace canvas::brush_lab;
  const auto set = makeReferenceBrushSet();
  assert(set.presets.size() == 5);
  std::set<std::uint64_t> renderHashes;
  for (const auto& preset : set.presets) {
    const auto resource = set.resources.renderResource(preset.definition.shapeResource,
                                                       preset.definition.grainResource);
    assert(resource != nullptr);
    renderHashes.insert(resource->contentHash);
  }
  assert(renderHashes.size() == set.presets.size());
  const auto manifest = referenceBrushManifestJson();
  assert(manifest.find("\"brush_count\":5") != std::string::npos);
  assert(manifest.find("RB-03 Dry Chalk") != std::string::npos);
  for (const auto& preset : set.presets) {
    assert(preset.definition.version == 2);
    assert(preset.definition.shapeResource.valid());
    assert(preset.definition.grainResource.valid());
    const auto compiled = compileQualificationBrush(preset, set.resources);
    assert(compiled);
    assert(compiled.program->representation() ==
           canvas::ink::BrushRepresentation::kDab);
  }

  const auto* rb03 = set.find(ReferenceBrushId::kDryChalk);
  assert(rb03 != nullptr);
  const auto original = compileQualificationBrush(*rb03, set.resources);
  const auto editedPreset = editDryChalk(*rb03, {.nominalSize = 19.0F,
                                                  .spacing = 0.34F,
                                                  .opacity = 0.62F});
  const auto edited = compileQualificationBrush(editedPreset, set.resources);
  assert(original && edited);
  assert(original.program->identity() != edited.program->identity());
  const auto restored = compileQualificationBrush(*rb03, set.resources);
  assert(restored.program->identity() == original.program->identity());

  const std::array firstStroke{
      canvas::ink::BrushInputSample{20.0F, 20.0F, 1.0F, 0.0F, 0.0F, 1},
      canvas::ink::BrushInputSample{80.0F, 40.0F, 1.0F, 0.0F, 0.0F, 2}};
  const auto first = runReferenceBrushStroke(*rb03, set.resources, firstStroke, 1);
  assert(!first.preview.empty());
  assert(!first.commit.primitives.empty());
  const auto second = runReferenceBrushStroke(*rb03, set.resources, firstStroke, 2);
  assert(second.commit.primitives.size() == first.commit.primitives.size());
  assert(second.commit.primitives.front().x == firstStroke.front().x);
  assert(second.commit.primitives.front().y == firstStroke.front().y);
  const auto editedPresetForStroke = editDryChalk(*rb03,
      {.nominalSize = 19.0F, .spacing = 0.34F, .opacity = 0.62F});
  const auto editedStroke = runReferenceBrushStroke(editedPresetForStroke,
      set.resources, firstStroke, 3);
  assert(editedStroke.commit.digest != first.commit.digest);

  ReferenceBrushStrokeSession session(*rb03, set.resources, 9);
  assert(session.valid());
  const auto firstAppend = session.append(std::span(firstStroke).first<1>());
  assert(firstAppend);
  const auto secondAppend = session.append(std::span(firstStroke).last<1>());
  assert(secondAppend);
  assert(secondAppend.preview.primitives.size() >= firstAppend.preview.primitives.size());
  assert(!session.finish().commit.primitives.empty());
}
