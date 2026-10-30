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

  package.revision = 3U;
  package.shape.resourceId = "chalk-shape-screenshot-extract-v1";
  package.shape.resourceSha256 = std::string(64, 'b');
  package.shape.resourceVersion = 1U;
  intent.revision = 4U;
  const auto v3Operation = canvas::ink_playground::BrushCommitAdapter::build(
      intent, package, 13U,
      canvas::semantic::DocumentId(canvas::foundation::ObjectId::fromUint64(9U)));
  const auto* v3Add = std::get_if<canvas::semantic::AddStrokeOp>(&v3Operation.payload);
  assert(v3Add != nullptr);
  const auto* v3Content = std::get_if<canvas::semantic::DabBrushStrokeContent>(&v3Add->object.content);
  assert(v3Content != nullptr);
  assert(v3Content->stroke.snapshot.package_revision == 3U);
  assert(v3Content->stroke.snapshot.resources.size() == 2U);
  assert(v3Content->stroke.snapshot.resources[0].kind == 1U);
  assert(v3Content->stroke.snapshot.resources[1].kind == 2U);

  package.profileId = "membrane-v1";
  package.material = canvas::ink::BrushMaterialMode::kMembrane;
  package.revision = 1U;
  package.shape.resourceId = "membrane-shape";
  package.shape.resourceSha256 = std::string(64, 'c');
  package.grain.resourceId = "membrane-grain";
  package.grain.resourceSha256 = std::string(64, 'd');
  assert(canvas::ink_playground::BrushCommitAdapter::valid(intent, package));
  const auto membraneOperation = canvas::ink_playground::BrushCommitAdapter::build(
      intent, package, 14U,
      canvas::semantic::DocumentId(canvas::foundation::ObjectId::fromUint64(9U)));
  const auto* membraneAdd = std::get_if<canvas::semantic::AddStrokeOp>(&membraneOperation.payload);
  assert(membraneAdd != nullptr);
  const auto* membraneContent = std::get_if<canvas::semantic::DabBrushStrokeContent>(&membraneAdd->object.content);
  assert(membraneContent != nullptr);
  assert(membraneContent->stroke.snapshot.profile_id == 4U);
  assert(membraneContent->stroke.snapshot.material_mode == 4U);

  auto invalid = intent;
  invalid.outline[1].x = std::numeric_limits<double>::quiet_NaN();
  assert(!canvas::ink_playground::BrushCommitAdapter::valid(invalid, package));
  invalid = intent;
  invalid.session = 0U;
  assert(!canvas::ink_playground::BrushCommitAdapter::valid(invalid, package));
  return 0;
}
