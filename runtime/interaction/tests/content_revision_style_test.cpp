#include "canvas/interaction/canvas_editing_runtime.hpp"

#include <cassert>

namespace {
class InspectRuntime final : public canvas::interaction::CanvasEditingRuntime {
 public:
  canvas::foundation::Result<canvas::CompiledSceneSnapshot> compile() {
    return sceneCompiler_->compileFull(canvas::semantic::SemanticReadView(
        semanticObjects_, semanticGeneration_.current()));
  }
};

canvas::semantic::ObjectRecord shape(std::uint64_t id) {
  canvas::semantic::ObjectRecord value;
  value.id = canvas::foundation::ObjectId::fromUint64(id);
  value.kind = canvas::semantic::ObjectKind::kShape;
  value.kind_version = 1U;
  value.placement.order_key = canvas::semantic::OrderKey{{1U}};
  value.content = canvas::semantic::ShapeContent{1U, 100.0, 80.0};
  return value;
}
}

int main() {
  InspectRuntime runtime;
  assert(runtime.bindSurface(256U, 256U));
  const auto id = canvas::foundation::ObjectId::fromUint64(100U);
  canvas::semantic::Operation insert{
      canvas::semantic::OperationId{canvas::foundation::ObjectId::fromUint64(1U)},
      canvas::semantic::DocumentId{canvas::foundation::ObjectId::fromUint64(1U)},
      1U, 1U, canvas::semantic::InsertObjectsOp{{shape(100U)}}};
  assert(runtime.applyExternalOperation(insert, canvas::semantic::ApplySource::kRemoteSync).disposition ==
         canvas::semantic::ApplyDisposition::kApplied);
  auto compiled = runtime.compile();
  assert(compiled);
  canvas::ContentRevision baseline{};
  for (const auto& record : compiled.value().records)
    if (record.objectId == id) baseline = record.contentRevision;

  canvas::semantic::SolidStroke stroke{
      {0.1F, 0.2F, 0.3F, 1.0F}, 2.0, canvas::semantic::StrokeCap::kButt,
      canvas::semantic::MiterJoin{4.0}, canvas::semantic::SolidDash{}};
  auto applyStyle = [&](std::uint64_t ordinal) {
    canvas::semantic::Operation operation{
        canvas::semantic::OperationId{canvas::foundation::ObjectId::fromUint64(ordinal)},
        canvas::semantic::DocumentId{canvas::foundation::ObjectId::fromUint64(1U)},
        1U, 1U,
        canvas::semantic::PatchPropertiesOp{{{id, 0x101U,
            canvas::semantic::PropertyPatchAction::kSet,
            canvas::semantic::PropertyValue{canvas::semantic::StrokeStyleValue{stroke}}}}}};
    assert(runtime.applyExternalOperation(operation, canvas::semantic::ApplySource::kRemoteSync).disposition ==
           canvas::semantic::ApplyDisposition::kApplied);
  };
  applyStyle(2U);
  compiled = runtime.compile();
  assert(compiled);
  for (const auto& record : compiled.value().records)
    if (record.objectId == id) baseline = record.contentRevision;
  stroke.join = canvas::semantic::MiterJoin{9.0};
  applyStyle(3U);
  compiled = runtime.compile();
  assert(compiled);
  for (const auto& record : compiled.value().records)
    if (record.objectId == id) assert(record.contentRevision != baseline);
  return 0;
}
