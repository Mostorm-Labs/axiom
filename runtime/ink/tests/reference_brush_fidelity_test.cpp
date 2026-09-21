#include "canvas/ink/brush_resource.hpp"
#include "canvas/ink/programmable_brush.hpp"

#include <array>
#include <cassert>
#include <cmath>

using namespace canvas::ink;

namespace {

BrushDefinition definition() {
  BrushDefinition value;
  value.definitionId = 9003;
  value.version = 2;
  value.family = BrushFamily::kChalk;
  value.nominalSize = 10.0F;
  value.opacity = 0.8F;
  value.spacing = 0.25F;
  value.shapeResource = {101};
  value.grainResource = {102};
  return value;
}

ResourceCatalog resources() {
  ResourceCatalog value;
  assert(value.add(makeProceduralShape({101}, 16, BrushResourcePattern::kBristle)));
  assert(value.add(makeProceduralGrain({102}, 16, BrushResourcePattern::kPaper)));
  return value;
}

std::vector<BrushPrimitive> stroke(std::span<const BrushInputSample> samples) {
  auto catalog = resources();
  const auto compiled = BrushCompiler{}.compile(
      definition(), {.pressure = true, .tilt = true, .shapeResource = true,
                     .grainResource = true});
  assert(compiled);
  BrushRuntime runtime(catalog);
  assert(runtime.begin({1}, *compiled.program, 77));
  assert(runtime.append({1}, samples));
  return runtime.finish({1}).commit.primitives;
}

}  // namespace

int main() {
  const auto compiled = BrushCompiler{}.compile(
      definition(), {.pressure = true, .tilt = true, .shapeResource = true,
                     .grainResource = true});
  assert(compiled);
  assert(compiled.program->definition().version == 2);

  auto catalog = resources();
  const auto derived = catalog.renderResource({101}, {102});
  assert(derived != nullptr);
  assert(derived->id.valid());
  assert(derived->contentHash != 0);
  assert(derived->alpha.size() == 256);
  assert(catalog.resource({101})->contentHash != 0);

  const std::array sparse{
      BrushInputSample{0, 0, 1, 0, 0, 1},
      BrushInputSample{10, 0, 1, 0, 0, 2},
      BrushInputSample{20, 0, 1, 0, 0, 3},
  };
  const std::array dense{
      BrushInputSample{0, 0, 1, 0, 0, 1},
      BrushInputSample{2, 0, 1, 0, 0, 2},
      BrushInputSample{4, 0, 1, 0, 0, 3},
      BrushInputSample{6, 0, 1, 0, 0, 4},
      BrushInputSample{8, 0, 1, 0, 0, 5},
      BrushInputSample{10, 0, 1, 0, 0, 6},
      BrushInputSample{12, 0, 1, 0, 0, 7},
      BrushInputSample{14, 0, 1, 0, 0, 8},
      BrushInputSample{16, 0, 1, 0, 0, 9},
      BrushInputSample{18, 0, 1, 0, 0, 10},
      BrushInputSample{20, 0, 1, 0, 0, 11},
  };
  const auto a = stroke(sparse);
  const auto b = stroke(dense);
  assert(a == b);
  assert(a.size() == 9);
  for (std::size_t i = 0; i < a.size(); ++i) {
    assert(a[i].dabOrdinal == i);
    assert(a[i].renderResource.valid());
    assert(std::abs(a[i].x - static_cast<float>(i) * 2.5F) < 0.001F);
  }
}
