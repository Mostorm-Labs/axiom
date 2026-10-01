// Diagnostic only: demonstrates why selecting a Vector reference policy is
// observable behavior. It is not an incremental implementation or C2 oracle.
#include "canvas/ink/programmable_brush.hpp"
#include "canvas/ink/vector_stroke_reference.hpp"

#include <cmath>
#include <iostream>
#include <span>
#include <vector>

int main() {
  namespace ink = canvas::ink;
  ink::BrushDefinition definition;
  definition.definitionId = 1;
  definition.nominalSize = 16;
  const auto program = ink::BrushCompiler{}.compile(definition, {});
  if (!program) return 1;
  const std::vector<ink::BrushInputSample> input{
      {0, 0, .2F, 0, 0, 1}, {20, 0, .4F, 0, 0, 2},
      {40, 8, .8F, 0, 0, 3}, {60, 16, .6F, 0, 0, 4}};
  ink::ResourceCatalog catalog;
  ink::BrushRuntime runtime(catalog);
  if (!runtime.begin({1}, *program.program, 42)) return 2;
  const auto evaluated = runtime.append({1}, input);
  if (!evaluated) return 3;
  std::vector<ink::reference::VectorStrokeInput> source;
  for (const auto& sample : input) {
    source.push_back({sample.x, sample.y, sample.pressure});
  }
  ink::reference::StrokeOptions defaults;
  defaults.size = definition.nominalSize;
  defaults.last = true;
  auto unthinned = defaults;
  unthinned.thinning = 0;
  const auto a = ink::reference::getStrokeOutlinePoints(
      ink::reference::getStrokePoints(source, defaults), defaults);
  const auto b = ink::reference::getStrokeOutlinePoints(
      ink::reference::getStrokePoints(source, unthinned), unthinned);
  if (a.empty() || b.empty()) return 4;
  const bool differing = a.size() != b.size() ||
      std::abs(a.front().x - b.front().x) > 1e-5 ||
      std::abs(a.front().y - b.front().y) > 1e-5;
  std::cout << "{\"kind\":\"diagnostic_not_acceptance\","
            << "\"legacy_primitive_count\":" << evaluated.preview.primitives.size()
            << ",\"r1_default_outline_count\":" << a.size()
            << ",\"r1_unthinned_outline_count\":" << b.size()
            << ",\"option_choice_changes_geometry\":"
            << (differing ? "true" : "false") << "}\n";
  return differing ? 0 : 5;
}
