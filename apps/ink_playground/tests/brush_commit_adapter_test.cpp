#include "brush_commit_adapter.hpp"

#include <cassert>
#include <limits>

int main() {
  canvas::ink::BrushPackage package;
  package.packageId = "vector-solid-v1";
  package.revision = 1U;
  canvas::ink::BrushCommitIntent intent;
  intent.session = 7U;
  intent.revision = 3U;
  intent.seed = 0x55U;
  intent.confirmed = {{1.0, 2.0, 0.5, true, 1U}, {3.0, 4.0, 0.6, true, 2U}};
  intent.outline = {{0.0, 0.0}, {4.0, 0.0}, {4.0, 4.0}};
  assert(canvas::ink_playground::BrushCommitAdapter::valid(intent, package));
  const auto operation = canvas::ink_playground::BrushCommitAdapter::build(
      intent, package, 11U,
      canvas::semantic::DocumentId(canvas::foundation::ObjectId::fromUint64(9U)));
  assert(operation.schema_version == 1U);
  assert(operation.payload_version == 2U);
  const auto* add = std::get_if<canvas::semantic::AddStrokeOp>(&operation.payload);
  assert(add != nullptr);
  assert(add->object.kind == canvas::semantic::ObjectKind::kVectorStroke);
  assert(add->object.kind_version == 2U);
  const auto* content = std::get_if<canvas::semantic::BrushStrokeContent>(&add->object.content);
  assert(content != nullptr);
  assert(content->stroke.snapshot.seed == intent.seed);
  assert(content->stroke.confirmed_samples.size() == intent.confirmed.size());
  assert(content->stroke.vector_output.outline.size() == intent.outline.size());

  package.profileId = "chalk-grain-v1";
  package.revision = 2U;
  package.material = canvas::ink::BrushMaterialMode::kChalkGrain;
  package.grain.resourceId = "chalk-grain-default";
  package.grain.resourceSha256 = std::string(64, 'a');
  package.grain.resourceVersion = 1U;
  intent.dabs = {{1.0, 2.0, 8.0, 0.1F, 0.5F}, {3.0, 4.0, 8.0, 0.2F, 0.45F}};
  intent.dabDigest = 0x1234U;
  assert(canvas::ink_playground::BrushCommitAdapter::valid(intent, package));
  const auto dabOperation = canvas::ink_playground::BrushCommitAdapter::build(
      intent, package, 12U,
      canvas::semantic::DocumentId(canvas::foundation::ObjectId::fromUint64(9U)));
  const auto* dabAdd = std::get_if<canvas::semantic::AddStrokeOp>(&dabOperation.payload);
  assert(dabAdd != nullptr);
  assert(dabAdd->object.kind == canvas::semantic::ObjectKind::kDabStroke);
  assert(dabAdd->object.kind_version == 2U);
  assert(std::holds_alternative<canvas::semantic::DabBrushStrokeContent>(dabAdd->object.content));

  auto invalid = intent;
  invalid.outline[1].x = std::numeric_limits<double>::quiet_NaN();
  assert(!canvas::ink_playground::BrushCommitAdapter::valid(invalid, package));
  invalid = intent;
  invalid.session = 0U;
  assert(!canvas::ink_playground::BrushCommitAdapter::valid(invalid, package));
  return 0;
}
