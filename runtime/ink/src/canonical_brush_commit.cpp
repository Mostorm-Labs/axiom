#include "canvas/ink/canonical_brush_commit.hpp"

namespace canvas::ink {
namespace {

semantic::PiecewiseLinearCurve01 identityCurve() {
  return {{{0.0F, 0.0F}, {1.0F, 1.0F}}};
}

semantic::BrushDescriptor canonicalDescriptor(const BrushDefinition& definition,
                                               BrushRepresentation representation) {
  semantic::BrushDescriptor result;
  result.brush_version = 1;
  result.color = {0.05F, 0.10F, 0.20F, 1.0F};
  result.nominal_size = definition.nominalSize;
  result.opacity = definition.opacity;
  result.smoothing.amount = 0.0F;
  result.spacing.normalized_spacing = definition.spacing;
  const bool pressureEnabled = definition.pressureSizeInfluence > 0.0F ||
                               definition.pressureOpacityInfluence > 0.0F;
  result.pressure.enabled = pressureEnabled;
  if (pressureEnabled) {
    result.pressure.size_curve = identityCurve();
    result.pressure.opacity_curve = identityCurve();
  }
  if (representation == BrushRepresentation::kDab) {
    result.brush_family_id = 3;
    result.blend_mode = semantic::BrushBlendMode::kNormal;
    result.texture_resource_id = semantic::ResourceId{
        canvas::foundation::ObjectId::fromUint64(definition.grainResource.value)};
  } else if (definition.family == BrushFamily::kHighlighter) {
    result.brush_family_id = 2;
    result.blend_mode = semantic::BrushBlendMode::kHighlighter;
  } else {
    result.brush_family_id = 1;
    result.blend_mode = semantic::BrushBlendMode::kNormal;
  }
  return result;
}

}  // namespace

std::optional<semantic::StrokeRecord> toCanonicalStroke(
    const BrushProgram& program, const BrushCommitArtifact& artifact) {
  if (!artifact.canonicalMutation || artifact.primitives.empty() ||
      program.representation() == BrushRepresentation::kTemporalTransient) {
    return std::nullopt;
  }
  semantic::StrokeRecord result;
  result.brush = canonicalDescriptor(program.definition(), program.representation());
  result.deterministic_seed = artifact.deterministicSeed;
  if (program.representation() == BrushRepresentation::kVector) {
    semantic::VectorStrokeData data;
    data.samples.reserve(artifact.sourceSamples.size());
    for (const auto& sample : artifact.sourceSamples) {
      data.samples.push_back({{sample.x, sample.y}, sample.pressure, {0.0, 0.0}});
    }
    if (data.samples.empty()) return std::nullopt;
    result.data = std::move(data);
  } else {
    semantic::DabStrokeData data;
    data.dabs.reserve(artifact.primitives.size());
    for (const auto& primitive : artifact.primitives) {
      data.dabs.push_back({{primitive.x, primitive.y}, primitive.size,
                           primitive.rotation, primitive.opacity});
    }
    result.data = std::move(data);
  }
  return result;
}

}  // namespace canvas::ink
