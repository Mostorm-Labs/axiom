#pragma once

#include "canvas/semantic/object_record.hpp"

#include <cstdint>
#include <variant>
#include <vector>

namespace canvas::semantic {

struct InsertObjectsOp final { std::vector<ObjectRecord> objects; bool operator==(const InsertObjectsOp&) const = default; };
struct DeleteObjectsOp final { std::vector<ObjectId> object_ids; bool operator==(const DeleteObjectsOp&) const = default; };
struct RestoreObjectsOp final { std::vector<ObjectRecord> objects; bool operator==(const RestoreObjectsOp&) const = default; };

struct PlacementItem final { ObjectId object_id{}; Placement placement{}; bool operator==(const PlacementItem&) const = default; };
struct SetPlacementsOp final { std::vector<PlacementItem> items; bool operator==(const SetPlacementsOp&) const = default; };
struct TransformItem final { ObjectId object_id{}; Transform2D transform{}; bool operator==(const TransformItem&) const = default; };
struct SetTransformsOp final { std::vector<TransformItem> items; bool operator==(const SetTransformsOp&) const = default; };

enum class PropertyPatchAction : std::uint8_t { kInvalid = 0, kSet = 1, kClear = 2 };
struct PropertyPatch final {
    ObjectId object_id{};
    std::uint32_t field_id = 0;
    PropertyPatchAction action = PropertyPatchAction::kInvalid;
    std::variant<std::monostate, PropertyValue> value{};
    bool operator==(const PropertyPatch&) const = default;
};
struct PatchPropertiesOp final { std::vector<PropertyPatch> patches; bool operator==(const PatchPropertiesOp&) const = default; };

struct ObjectSizeItem final { ObjectId object_id{}; double width = 0.0; double height = 0.0; bool operator==(const ObjectSizeItem&) const = default; };
struct SetObjectSizeOp final { std::vector<ObjectSizeItem> items; bool operator==(const SetObjectSizeOp&) const = default; };
struct SetVectorPathGeometryOp final { ObjectId object_id{}; VectorPathGeometry geometry{}; bool operator==(const SetVectorPathGeometryOp&) const = default; };
struct SetImageContentOp final { ObjectId object_id{}; ImageContent content{}; bool operator==(const SetImageContentOp&) const = default; };
struct AddStrokeOp final { ObjectRecord object{}; bool operator==(const AddStrokeOp&) const = default; };

struct StrokeSplit final { ObjectId source_stroke_id{}; std::vector<ObjectRecord> replacements; bool operator==(const StrokeSplit&) const = default; };
struct SplitStrokesOp final { std::vector<StrokeSplit> splits; bool operator==(const SplitStrokesOp&) const = default; };
struct EraseMaskAddItem final { ObjectId object_id{}; std::vector<EraseMaskRecord> masks; bool operator==(const EraseMaskAddItem&) const = default; };
struct AddEraseMasksOp final { std::vector<EraseMaskAddItem> items; bool operator==(const AddEraseMasksOp&) const = default; };
struct EraseMaskRemoveItem final { ObjectId object_id{}; std::vector<ObjectId> mask_ids; bool operator==(const EraseMaskRemoveItem&) const = default; };
struct RemoveEraseMasksOp final { std::vector<EraseMaskRemoveItem> items; bool operator==(const RemoveEraseMasksOp&) const = default; };
struct EditRichTextOp final { ObjectId object_id{}; RichTextDelta delta{}; bool operator==(const EditRichTextOp&) const = default; };
struct SetConnectorContentOp final { ObjectId object_id{}; ConnectorContent content{}; bool operator==(const SetConnectorContentOp&) const = default; };

using OperationPayload = std::variant<
    InsertObjectsOp, DeleteObjectsOp, RestoreObjectsOp, SetPlacementsOp,
    SetTransformsOp, PatchPropertiesOp, SetObjectSizeOp,
    SetVectorPathGeometryOp, SetImageContentOp, AddStrokeOp, SplitStrokesOp,
    AddEraseMasksOp, RemoveEraseMasksOp, EditRichTextOp, SetConnectorContentOp>;

} // namespace canvas::semantic
