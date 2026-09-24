#include "canvas/semantic/brush_engine_codec.hpp"

#include "canvas/semantic/object_content.hpp"

namespace canvas::semantic {

bool BrushEngineCodec::acceptsNewObject(const ObjectRecord& object) noexcept {
    return object.kind == ObjectKind::kVectorStroke && object.kind_version == 2U &&
           std::holds_alternative<BrushStrokeContent>(object.content);
}

bool BrushEngineCodec::acceptsNewAddStroke(const Operation& operation) noexcept {
    if (operation.kind() != OperationKind::kAddStroke || operation.schema_version != 1U ||
        operation.payload_version != 2U) return false;
    const auto* payload = std::get_if<AddStrokeOp>(&operation.payload);
    return payload != nullptr && acceptsNewObject(payload->object);
}

bool BrushEngineCodec::rejectsLegacyObjectAsNewAuthoring(const ObjectRecord& object) noexcept {
    return object.kind == ObjectKind::kVectorStroke && object.kind_version == 1U &&
           std::holds_alternative<VectorStrokeContent>(object.content);
}

}  // namespace canvas::semantic
