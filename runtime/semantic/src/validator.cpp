#include "canvas/semantic/validator.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace canvas::semantic {
namespace {

bool validId(const ObjectId& value) noexcept { return !value.isZero(); }

template <typename Range, typename Key>
bool validCanonicalSet(const Range& values, Key key) {
    if (values.empty()) return false;
    for (std::size_t index = 0; index < values.size(); ++index) {
        const auto current = key(values[index]);
        if constexpr (std::is_same_v<std::decay_t<decltype(current)>, ObjectId>) {
            if (!validId(current)) return false;
        }
        if (index > 0U && !(key(values[index - 1U]) < current)) return false;
    }
    return true;
}

bool validPropertyValue(std::uint32_t field_id, const PropertyValue& value) noexcept {
    switch (field_id) {
        case 0x00000001U:
        case 0x00000002U:
            return std::holds_alternative<bool>(value);
        case 0x00000003U: {
            const auto* opacity = std::get_if<float>(&value);
            return opacity != nullptr && std::isfinite(*opacity) && *opacity >= 0.0F && *opacity <= 1.0F;
        }
        case 0x00000004U:
            return std::holds_alternative<BlendModeValue>(value) &&
                   std::get<BlendModeValue>(value) == BlendModeValue::kNormal;
        case 0x00000100U:
            return std::holds_alternative<FillStyleValue>(value);
        case 0x00000101U:
            return std::holds_alternative<StrokeStyleValue>(value);
        case 0x00000200U:
        case 0x00000201U:
            if (const auto* decoration = std::get_if<ConnectorDecorationValue>(&value)) {
                return *decoration == ConnectorDecorationValue::kNone ||
                       *decoration == ConnectorDecorationValue::kArrow;
            }
            return false;
        default:
            return false;
    }
}

bool validPropertyBag(const PropertyBag& bag) {
    if (bag.entries.empty()) return true;
    for (std::size_t index = 0; index < bag.entries.size(); ++index) {
        const auto& entry = bag.entries[index];
        if (!validPropertyValue(entry.field_id, entry.value)) return false;
        if (index > 0U && bag.entries[index - 1U].field_id >= entry.field_id) return false;
    }
    return true;
}

bool validObjectKindTriple(const ObjectRecord& object) noexcept {
    if (!validId(object.id) || object.kind_version != 1U) return false;
    switch (object.kind) {
        case ObjectKind::kShape:
            return std::holds_alternative<ShapeContent>(object.content);
        case ObjectKind::kImage:
            return std::holds_alternative<ImageContent>(object.content);
        case ObjectKind::kVectorPath:
            return std::holds_alternative<VectorPathContent>(object.content);
        case ObjectKind::kRichText:
            return std::holds_alternative<RichTextContent>(object.content);
        case ObjectKind::kVectorStroke:
            return std::holds_alternative<VectorStrokeContent>(object.content);
        case ObjectKind::kDabStroke:
            return std::holds_alternative<DabStrokeContent>(object.content);
        case ObjectKind::kConnector:
            return std::holds_alternative<ConnectorContent>(object.content);
        case ObjectKind::kSticky:
            return std::holds_alternative<StickyContent>(object.content);
        case ObjectKind::kGroup:
            return std::holds_alternative<GroupContent>(object.content);
    }
    return false;
}

bool validObjectRecordStructure(const ObjectRecord& object) {
    if (!validObjectKindTriple(object) || !validPropertyBag(object.properties)) return false;
    if (object.placement.parent_id.has_value() && !validId(*object.placement.parent_id)) return false;
    if (!object.placement.order_key.isValid()) return false;
    if (!object.erase_masks.empty()) {
        if (!validCanonicalSet(object.erase_masks, [](const EraseMaskRecord& mask) { return mask.id; })) {
            return false;
        }
    }
    return true;
}

bool validPatch(const PropertyPatch& patch) noexcept {
    if (!validId(patch.object_id)) return false;
    const auto* value = std::get_if<PropertyValue>(&patch.value);
    switch (patch.action) {
        case PropertyPatchAction::kSet:
            return value != nullptr && validPropertyValue(patch.field_id, *value);
        case PropertyPatchAction::kClear:
            return value == nullptr && patch.field_id != 0U &&
                   (patch.field_id == 0x00000001U || patch.field_id == 0x00000002U ||
                    patch.field_id == 0x00000003U || patch.field_id == 0x00000004U ||
                    patch.field_id == 0x00000100U || patch.field_id == 0x00000101U ||
                    patch.field_id == 0x00000200U || patch.field_id == 0x00000201U);
        case PropertyPatchAction::kInvalid:
            return false;
    }
    return false;
}

bool allUnique(std::vector<ObjectId> values) {
    std::sort(values.begin(), values.end());
    return std::adjacent_find(values.begin(), values.end()) == values.end();
}

ValidationResult invalidCollection() noexcept {
    return {ValidationIssue::kInvalidCollection};
}

ValidationResult invalidObjectKind() noexcept {
    return {ValidationIssue::kInvalidObjectKind};
}

ValidationResult invalidPropertyPatch() noexcept {
    return {ValidationIssue::kInvalidPropertyPatch};
}

} // namespace

ValidationResult validateEnvelope(
    const Operation& operation,
    const OperationFieldPresence& presence) noexcept {
    if (operation.id.isZero() || operation.document_id.isZero()) {
        return {ValidationIssue::kInvalidId};
    }
    if (!presence.schema_version || !presence.payload_version ||
        operation.schema_version != 1U || operation.payload_version != 1U) {
        return {ValidationIssue::kUnsupportedVersion};
    }
    return {};
}

ValidationResult validatePayloadStructure(const Operation& operation) noexcept {
    return std::visit(
        [](const auto& payload) -> ValidationResult {
            using Payload = std::decay_t<decltype(payload)>;
            if constexpr (std::is_same_v<Payload, InsertObjectsOp> ||
                          std::is_same_v<Payload, RestoreObjectsOp>) {
                if (!validCanonicalSet(payload.objects, [](const ObjectRecord& object) { return object.id; })) {
                    return invalidCollection();
                }
                for (const auto& object : payload.objects) {
                    if (!validObjectRecordStructure(object)) return invalidObjectKind();
                }
            } else if constexpr (std::is_same_v<Payload, DeleteObjectsOp>) {
                if (!validCanonicalSet(payload.object_ids, [](const ObjectId& value) { return value; })) {
                    return invalidCollection();
                }
            } else if constexpr (std::is_same_v<Payload, SetPlacementsOp>) {
                if (!validCanonicalSet(payload.items, [](const PlacementItem& item) { return item.object_id; })) {
                    return invalidCollection();
                }
                for (const auto& item : payload.items) {
                    if (!item.placement.order_key.isValid() ||
                        (item.placement.parent_id.has_value() && !validId(*item.placement.parent_id))) {
                        return invalidCollection();
                    }
                }
            } else if constexpr (std::is_same_v<Payload, SetTransformsOp>) {
                if (!validCanonicalSet(payload.items, [](const TransformItem& item) { return item.object_id; })) {
                    return invalidCollection();
                }
            } else if constexpr (std::is_same_v<Payload, PatchPropertiesOp>) {
                if (payload.patches.empty()) return invalidCollection();
                for (std::size_t index = 0; index < payload.patches.size(); ++index) {
                    const auto& patch = payload.patches[index];
                    if (!validPatch(patch)) return invalidPropertyPatch();
                    if (index > 0U) {
                        const auto& previous = payload.patches[index - 1U];
                        const bool ordered = previous.object_id < patch.object_id ||
                                             (previous.object_id == patch.object_id &&
                                              previous.field_id < patch.field_id);
                        if (!ordered) return invalidCollection();
                    }
                }
            } else if constexpr (std::is_same_v<Payload, SetObjectSizeOp>) {
                if (!validCanonicalSet(payload.items, [](const ObjectSizeItem& item) { return item.object_id; })) {
                    return invalidCollection();
                }
            } else if constexpr (std::is_same_v<Payload, SetVectorPathGeometryOp> ||
                                 std::is_same_v<Payload, SetImageContentOp> ||
                                 std::is_same_v<Payload, EditRichTextOp> ||
                                 std::is_same_v<Payload, SetConnectorContentOp>) {
                if (!validId(payload.object_id)) return invalidCollection();
            } else if constexpr (std::is_same_v<Payload, AddStrokeOp>) {
                if (!validObjectRecordStructure(payload.object)) return invalidObjectKind();
            } else if constexpr (std::is_same_v<Payload, SplitStrokesOp>) {
                if (!validCanonicalSet(payload.splits, [](const StrokeSplit& split) { return split.source_stroke_id; })) {
                    return invalidCollection();
                }
                std::vector<ObjectId> replacement_ids;
                for (const auto& split : payload.splits) {
                    if (!validCanonicalSet(split.replacements, [](const ObjectRecord& object) { return object.id; })) {
                        return invalidCollection();
                    }
                    for (const auto& replacement : split.replacements) {
                        if (!validObjectRecordStructure(replacement)) return invalidObjectKind();
                        replacement_ids.push_back(replacement.id);
                    }
                }
                if (!allUnique(std::move(replacement_ids))) return invalidCollection();
            } else if constexpr (std::is_same_v<Payload, AddEraseMasksOp>) {
                if (!validCanonicalSet(payload.items, [](const EraseMaskAddItem& item) { return item.object_id; })) {
                    return invalidCollection();
                }
                std::vector<ObjectId> mask_ids;
                for (const auto& item : payload.items) {
                    if (!validCanonicalSet(item.masks, [](const EraseMaskRecord& mask) { return mask.id; })) {
                        return invalidCollection();
                    }
                    for (const auto& mask : item.masks) mask_ids.push_back(mask.id);
                }
                if (!allUnique(std::move(mask_ids))) return invalidCollection();
            } else if constexpr (std::is_same_v<Payload, RemoveEraseMasksOp>) {
                if (!validCanonicalSet(payload.items, [](const EraseMaskRemoveItem& item) { return item.object_id; })) {
                    return invalidCollection();
                }
                for (const auto& item : payload.items) {
                    if (!validCanonicalSet(item.mask_ids, [](const ObjectId& value) { return value; })) {
                        return invalidCollection();
                    }
                }
            }
            return {};
        },
        operation.payload);
}

} // namespace canvas::semantic
