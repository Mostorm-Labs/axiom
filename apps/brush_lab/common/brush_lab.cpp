#include "brush_lab.hpp"

#include <array>
#include <iomanip>
#include <sstream>

namespace canvas::brush_lab {
namespace {

std::string_view nameOf(ink::BrushFamily family) {
  switch (family) {
    case ink::BrushFamily::kPen: return "pen";
    case ink::BrushFamily::kPencil: return "pencil";
    case ink::BrushFamily::kChalk: return "chalk";
    case ink::BrushFamily::kMarker: return "marker";
    case ink::BrushFamily::kWaterColorLite: return "water_color_lite";
    case ink::BrushFamily::kHighlighter: return "highlighter";
    case ink::BrushFamily::kLaser: return "laser";
  }
  return {};
}

bool dabFamily(ink::BrushFamily family) {
  return family == ink::BrushFamily::kPencil || family == ink::BrushFamily::kChalk ||
         family == ink::BrushFamily::kMarker ||
         family == ink::BrushFamily::kWaterColorLite;
}

ink::BrushDefinition definitionFor(ink::BrushFamily family) {
  ink::BrushDefinition definition;
  definition.definitionId = static_cast<std::uint64_t>(family);
  definition.family = family;
  definition.nominalSize = family == ink::BrushFamily::kHighlighter ? 18.0F : 7.0F;
  definition.opacity = family == ink::BrushFamily::kWaterColorLite ? 0.35F : 0.8F;
  definition.spacing = dabFamily(family) ? 0.2F : 0.08F;
  definition.pressureSizeInfluence = 0.55F;
  definition.pressureOpacityInfluence = 0.2F;
  if (dabFamily(family)) {
    definition.shapeResource = {100 + static_cast<std::uint64_t>(family)};
    definition.grainResource = {200 + static_cast<std::uint64_t>(family)};
  }
  return definition;
}

ink::BrushCapabilityProfile capabilities() {
  return {.pressure = true, .tilt = true, .shapeResource = true,
          .grainResource = true, .temporalTransient = true};
}

}  // namespace

Scenario evaluate(ink::BrushFamily family) {
  const auto definition = definitionFor(family);
  const auto compiled = ink::BrushCompiler{}.compile(definition, capabilities());
  if (!compiled) return {};
  ink::ResourceCatalog catalog;
  if (definition.shapeResource.valid()) {
    catalog.add(definition.shapeResource, ink::BrushResourceKind::kShape);
  }
  if (definition.grainResource.valid()) {
    catalog.add(definition.grainResource, ink::BrushResourceKind::kGrain);
  }
  ink::BrushRuntime runtime(catalog);
  const ink::BrushSessionId session{static_cast<std::uint64_t>(family)};
  if (!runtime.begin(session, *compiled.program, 0x4500ULL + session.value)) return {};
  const std::array samples{
      ink::BrushInputSample{8, 30, 0.2F, 0, 0, 1},
      ink::BrushInputSample{32, 16, 0.4F, 0, 0, 2},
      ink::BrushInputSample{56, 24, 0.6F, 0, 0, 3},
      ink::BrushInputSample{80, 12, 0.8F, 0, 0, 4},
      ink::BrushInputSample{104, 28, 1.0F, 0, 0, 5}};
  if (!runtime.append(session, samples)) return {};
  const auto finished = runtime.finish(session);
  if (!finished) return {};

  std::ostringstream svg;
  svg << "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"120\" height=\"40\" "
         "viewBox=\"0 0 120 40\" data-brush=\""
      << nameOf(family) << "\">";
  svg << std::fixed << std::setprecision(3);
  for (const auto& primitive : finished.preview.primitives) {
    svg << "<circle cx=\"" << primitive.x << "\" cy=\"" << primitive.y
        << "\" r=\"" << primitive.size * 0.5F << "\" opacity=\""
        << primitive.opacity << "\"/>";
  }
  svg << "</svg>";
  const auto count = finished.preview.primitives.size();
  return {std::string(nameOf(family)), compiled.program->identity(),
          finished.commit.digest, count, finished.commit.canonicalMutation,
          svg.str(), {samples.size(), count, count * 40U}};
}

std::string manifestJson() {
  std::ostringstream out;
  out << "{\"schema_version\":\"0.1\",\"scenario_count\":7,\"scenarios\":[";
  bool first = true;
  for (const auto family : {ink::BrushFamily::kPen, ink::BrushFamily::kPencil,
                            ink::BrushFamily::kChalk, ink::BrushFamily::kMarker,
                            ink::BrushFamily::kWaterColorLite,
                            ink::BrushFamily::kHighlighter,
                            ink::BrushFamily::kLaser}) {
    const auto scenario = evaluate(family);
    if (!first) out << ',';
    first = false;
    out << "{\"name\":\"" << scenario.name << "\",\"program_identity\":"
        << scenario.programIdentity << ",\"commit_digest\":"
        << scenario.commitDigest << ",\"primitive_count\":"
        << scenario.primitiveCount << ",\"canonical_mutation\":"
        << (scenario.canonicalMutation ? "true" : "false")
        << ",\"estimated_bytes\":" << scenario.workload.estimatedBytes << '}';
  }
  out << "]}";
  return out.str();
}

}  // namespace canvas::brush_lab
