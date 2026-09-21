#include "canvas/ink/programmable_brush.hpp"

#include <array>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <limits>
#include <span>
#include <vector>

namespace ink = canvas::ink;

namespace {

ink::BrushDefinition makeDefinition(ink::BrushFamily family) {
  ink::BrushDefinition definition;
  definition.definitionId = static_cast<std::uint64_t>(family);
  definition.version = 1;
  definition.family = family;
  definition.nominalSize = 8.0F;
  definition.opacity = 0.75F;
  definition.spacing = 0.25F;
  definition.pressureSizeInfluence = 0.5F;
  definition.pressureOpacityInfluence = 0.25F;
  return definition;
}

ink::BrushCapabilityProfile fullCapabilities() {
  return {.pressure = true, .tilt = true, .shapeResource = true,
          .grainResource = true, .temporalTransient = true};
}

void definitionAndCompilerContract() {
  ink::BrushCompiler compiler;
  const auto pen = compiler.compile(makeDefinition(ink::BrushFamily::kPen),
                                    fullCapabilities());
  assert(pen);
  assert(pen.program->representation() == ink::BrushRepresentation::kVector);
  assert(pen.program->identity() != 0);

  const auto repeated = compiler.compile(makeDefinition(ink::BrushFamily::kPen),
                                         fullCapabilities());
  assert(repeated);
  assert(repeated.program->identity() == pen.program->identity());

  auto pencilDefinition = makeDefinition(ink::BrushFamily::kPencil);
  pencilDefinition.shapeResource = ink::ResourceId{11};
  pencilDefinition.grainResource = ink::ResourceId{12};
  const auto pencil = compiler.compile(pencilDefinition, fullCapabilities());
  assert(pencil);
  assert(pencil.program->representation() == ink::BrushRepresentation::kDab);
  assert(pencil.program->identity() != pen.program->identity());

  const auto laser = compiler.compile(makeDefinition(ink::BrushFamily::kLaser),
                                      fullCapabilities());
  assert(laser);
  assert(laser.program->representation() ==
         ink::BrushRepresentation::kTemporalTransient);

  auto invalid = makeDefinition(ink::BrushFamily::kMarker);
  invalid.opacity = std::numeric_limits<float>::quiet_NaN();
  assert(compiler.compile(invalid, fullCapabilities()).error ==
         ink::BrushCompileError::kInvalidDefinition);

  auto unavailable = makeDefinition(ink::BrushFamily::kChalk);
  unavailable.shapeResource = ink::ResourceId{21};
  unavailable.grainResource = ink::ResourceId{22};
  const auto noResources = compiler.compile(
      unavailable, {.pressure = true, .tilt = false, .shapeResource = false,
                    .grainResource = false, .temporalTransient = true});
  assert(noResources.error == ink::BrushCompileError::kUnsupportedCapability);

  auto wrongVersion = makeDefinition(ink::BrushFamily::kPen);
  wrongVersion.version = 2;
  assert(compiler.compile(wrongVersion, fullCapabilities()).error ==
         ink::BrushCompileError::kUnsupportedVersion);
}

void resourceAndRuntimeContract() {
  ink::BrushCompiler compiler;
  auto markerDefinition = makeDefinition(ink::BrushFamily::kMarker);
  markerDefinition.shapeResource = ink::ResourceId{31};
  markerDefinition.grainResource = ink::ResourceId{32};
  const auto compiled = compiler.compile(markerDefinition, fullCapabilities());
  assert(compiled);

  ink::ResourceCatalog resources;
  resources.add({31}, ink::BrushResourceKind::kShape);
  resources.add({32}, ink::BrushResourceKind::kGrain);
  ink::BrushRuntime runtime(resources);
  assert(runtime.begin(ink::BrushSessionId{7}, *compiled.program, 12345));

  const std::array samples{
      ink::BrushInputSample{0.0F, 0.0F, 0.2F, 0.0F, 0.0F, 1},
      ink::BrushInputSample{8.0F, 0.0F, 0.7F, 0.0F, 0.0F, 2},
      ink::BrushInputSample{16.0F, 4.0F, 1.0F, 0.0F, 0.0F, 3}};
  const auto update = runtime.append({7}, samples);
  assert(update);
  assert(!update.preview.primitives.empty());
  assert(update.commit.primitives.size() == update.preview.primitives.size());
  const auto finished = runtime.finish({7});
  assert(finished);
  assert(finished.commit.canonicalMutation);
  assert(finished.commit.representation == ink::BrushRepresentation::kDab);

  assert(runtime.begin({8}, *compiled.program, 12345));
  const auto chunkA = runtime.append({8}, std::span(samples).first(1));
  const auto chunkB = runtime.append({8}, std::span(samples).subspan(1));
  assert(chunkA && chunkB);
  const auto chunked = runtime.finish({8});
  assert(chunked);
  assert(chunked.commit.digest == finished.commit.digest);

  ink::ResourceCatalog missing;
  ink::BrushRuntime missingRuntime(missing);
  assert(!missingRuntime.begin({9}, *compiled.program, 3));
  assert(missingRuntime.lastError() == ink::BrushRuntimeError::kMissingResource);

  ink::ResourceCatalog mismatched;
  mismatched.add({31}, ink::BrushResourceKind::kGrain);
  mismatched.add({32}, ink::BrushResourceKind::kGrain);
  ink::BrushRuntime mismatchRuntime(mismatched);
  assert(!mismatchRuntime.begin({9}, *compiled.program, 3));
  assert(mismatchRuntime.lastError() == ink::BrushRuntimeError::kResourceKindMismatch);
}

void deterministicChannelsAndSessionIsolation() {
  const auto a0 = ink::deterministicChannel(99, ink::RandomChannel::kSize, 4);
  const auto a1 = ink::deterministicChannel(99, ink::RandomChannel::kSize, 4);
  const auto b = ink::deterministicChannel(99, ink::RandomChannel::kRotation, 4);
  assert(a0 == a1);
  assert(a0 != b);
  assert(a0 >= 0.0F && a0 < 1.0F);

  ink::BrushCompiler compiler;
  const auto pen = compiler.compile(makeDefinition(ink::BrushFamily::kPen),
                                    fullCapabilities());
  assert(pen);
  ink::ResourceCatalog resources;
  ink::BrushRuntime runtime(resources);
  assert(runtime.begin({100}, *pen.program, 1));
  assert(runtime.begin({101}, *pen.program, 2));
  const ink::BrushInputSample sample{1, 2, 0.5F, 0, 0, 1};
  assert(runtime.append({100}, std::span(&sample, 1)));
  assert(runtime.append({101}, std::span(&sample, 1)));
  const std::array invalidBatch{
      ink::BrushInputSample{2, 3, 0.5F, 0, 0, 2},
      ink::BrushInputSample{4, 5, 0.5F, 0, 0, 2}};
  assert(!runtime.append({101}, invalidBatch));
  const auto afterRejectedBatch = runtime.finish({101});
  assert(afterRejectedBatch);
  assert(afterRejectedBatch.preview.primitives.size() == 1);
  assert(runtime.cancel({100}));
  assert(!runtime.finish({100}));
}

void laserIsTemporalAndNeverCanonical() {
  ink::BrushCompiler compiler;
  const auto laser = compiler.compile(makeDefinition(ink::BrushFamily::kLaser),
                                      fullCapabilities());
  assert(laser);
  ink::ResourceCatalog resources;
  ink::BrushRuntime runtime(resources);
  assert(runtime.begin({200}, *laser.program, 77));
  const ink::BrushInputSample sample{2, 3, 1, 0, 0, 1};
  const auto update = runtime.append({200}, std::span(&sample, 1));
  assert(update);
  assert(!update.commit.canonicalMutation);
  const auto finished = runtime.finish({200});
  assert(finished);
  assert(!finished.commit.canonicalMutation);
  assert(finished.commit.primitives.empty());
  assert(finished.preview.durationMs > 0);
}

void allV1FamiliesCompile() {
  ink::BrushCompiler compiler;
  for (const auto family : {ink::BrushFamily::kPen, ink::BrushFamily::kPencil,
                            ink::BrushFamily::kChalk, ink::BrushFamily::kMarker,
                            ink::BrushFamily::kWaterColorLite,
                            ink::BrushFamily::kHighlighter,
                            ink::BrushFamily::kLaser}) {
    auto definition = makeDefinition(family);
    if (family == ink::BrushFamily::kPencil || family == ink::BrushFamily::kChalk ||
        family == ink::BrushFamily::kMarker ||
        family == ink::BrushFamily::kWaterColorLite) {
      definition.shapeResource = ink::ResourceId{100 + static_cast<std::uint64_t>(family)};
      definition.grainResource = ink::ResourceId{200 + static_cast<std::uint64_t>(family)};
    }
    assert(compiler.compile(definition, fullCapabilities()));
  }
}

}  // namespace

int main() {
  definitionAndCompilerContract();
  resourceAndRuntimeContract();
  deterministicChannelsAndSessionIsolation();
  laserIsTemporalAndNeverCanonical();
  allV1FamiliesCompile();
  return 0;
}
