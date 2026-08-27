#include "canvas/semantic/validator.hpp"

#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <utility>
#include <variant>

namespace canvas::semantic {
namespace {

ObjectId id(std::uint64_t value) { return ObjectId::fromUint64(value); }

StrokeRecord vectorStroke() {
    StrokeRecord stroke;
    stroke.brush.brush_family_id = 1U;
    stroke.brush.brush_version = 1U;
    stroke.brush.nominal_size = 1.0;
    stroke.brush.opacity = 1.0F;
    stroke.data = VectorStrokeData{{StrokeSample{{1.0, 2.0}, 0.5F, {0.0, 0.0}}}};
    return stroke;
}

StrokeRecord dabStroke() {
    StrokeRecord stroke;
    stroke.brush.brush_family_id = 3U;
    stroke.brush.brush_version = 1U;
    stroke.brush.nominal_size = 1.0;
    stroke.brush.opacity = 1.0F;
    stroke.brush.texture_resource_id = ResourceId{ObjectId::fromUint64(900U)};
    stroke.data = DabStrokeData{{DabInstance{{1.0, 2.0}, 2.0, 0.0F, 1.0F}}};
    return stroke;
}

ObjectRecord record(ObjectKind kind, std::uint64_t identity) {
    ObjectRecord value;
    value.id = id(identity);
    value.kind = kind;
    value.kind_version = 1U;
    value.placement.order_key = OrderKey({1U});
    switch (kind) {
        case ObjectKind::kShape:
            value.content = ShapeContent{1U, 10.0, 20.0};
            break;
        case ObjectKind::kImage:
            value.content = ImageContent{
                ResourceId{id(900U)}, 10.0, 20.0, std::nullopt,
                ImageContentMode::kStretch, 10.0, 20.0};
            break;
        case ObjectKind::kVectorPath:
            value.content = VectorPathContent{
                VectorPathGeometry{FillRule::kNonZero, {MoveTo{{0.0, 0.0}}}}};
            break;
        case ObjectKind::kRichText:
            value.content = RichTextContent{};
            break;
        case ObjectKind::kVectorStroke:
            value.content = VectorStrokeContent{vectorStroke()};
            break;
        case ObjectKind::kDabStroke:
            value.content = DabStrokeContent{dabStroke()};
            break;
        case ObjectKind::kConnector:
            value.content = ConnectorContent{
                ConnectorEndpoint{FreePointEndpoint{{0.0, 0.0}}},
                ConnectorEndpoint{FreePointEndpoint{{1.0, 1.0}}},
                ConnectorRouting::kStraight};
            break;
        case ObjectKind::kSticky:
            value.content = StickyContent{10.0, 20.0};
            break;
        case ObjectKind::kGroup:
            value.content = GroupContent{};
            break;
    }
    return value;
}

EraseMaskRecord mask(std::uint64_t identity) {
    return EraseMaskRecord{id(identity), SweptCircleMask{}};
}

Operation collectionCase(std::size_t profile, int mode) {
    const bool empty = mode == 0;
    const bool duplicate = mode == 1;
    Operation operation;
    switch (profile) {
        case 0: {
            InsertObjectsOp value;
            if (!empty) value.objects = {record(ObjectKind::kShape, 1U), record(ObjectKind::kShape, duplicate ? 1U : 2U)};
            operation.payload = std::move(value);
            break;
        }
        case 1: {
            DeleteObjectsOp value;
            if (!empty) value.object_ids = {id(1U), id(duplicate ? 1U : 2U)};
            operation.payload = std::move(value);
            break;
        }
        case 2: {
            RestoreObjectsOp value;
            if (!empty) value.objects = {record(ObjectKind::kShape, 1U), record(ObjectKind::kShape, duplicate ? 1U : 2U)};
            operation.payload = std::move(value);
            break;
        }
        case 3: {
            SetPlacementsOp value;
            if (!empty) value.items = {
                PlacementItem{id(1U), Placement{std::nullopt, OrderKey({1U})}},
                PlacementItem{id(duplicate ? 1U : 2U), Placement{std::nullopt, OrderKey({2U})}}};
            operation.payload = std::move(value);
            break;
        }
        case 4: {
            SetTransformsOp value;
            if (!empty) value.items = {
                TransformItem{id(1U), Transform2D{}},
                TransformItem{id(duplicate ? 1U : 2U), Transform2D{}}};
            operation.payload = std::move(value);
            break;
        }
        case 5: {
            PatchPropertiesOp value;
            if (!empty) value.patches = {
                PropertyPatch{id(1U), 1U, PropertyPatchAction::kSet, PropertyValue{true}},
                PropertyPatch{id(duplicate ? 1U : 2U), duplicate ? 1U : 2U,
                              PropertyPatchAction::kSet, PropertyValue{false}}};
            operation.payload = std::move(value);
            break;
        }
        case 6: {
            SetObjectSizeOp value;
            if (!empty) value.items = {
                ObjectSizeItem{id(1U), 10.0, 20.0},
                ObjectSizeItem{id(duplicate ? 1U : 2U), 30.0, 40.0}};
            operation.payload = std::move(value);
            break;
        }
        case 7: {
            SplitStrokesOp value;
            if (!empty) value.splits = {
                StrokeSplit{id(1U), {record(ObjectKind::kVectorStroke, 101U)}},
                StrokeSplit{id(duplicate ? 1U : 2U), {record(ObjectKind::kVectorStroke, 102U)}}};
            operation.payload = std::move(value);
            break;
        }
        case 8: {
            SplitStrokesOp value{{StrokeSplit{id(1U), {}}}};
            if (!empty) value.splits.front().replacements = {
                record(ObjectKind::kVectorStroke, 101U),
                record(ObjectKind::kVectorStroke, duplicate ? 101U : 102U)};
            operation.payload = std::move(value);
            break;
        }
        case 9: {
            AddEraseMasksOp value;
            if (!empty) value.items = {
                EraseMaskAddItem{id(1U), {mask(101U)}},
                EraseMaskAddItem{id(duplicate ? 1U : 2U), {mask(102U)}}};
            operation.payload = std::move(value);
            break;
        }
        case 10: {
            AddEraseMasksOp value{{EraseMaskAddItem{id(1U), {}}}};
            if (!empty) value.items.front().masks = {mask(101U), mask(duplicate ? 101U : 102U)};
            operation.payload = std::move(value);
            break;
        }
        case 11: {
            RemoveEraseMasksOp value;
            if (!empty) value.items = {
                EraseMaskRemoveItem{id(1U), {id(101U)}},
                EraseMaskRemoveItem{id(duplicate ? 1U : 2U), {id(102U)}}};
            operation.payload = std::move(value);
            break;
        }
        case 12: {
            RemoveEraseMasksOp value{{EraseMaskRemoveItem{id(1U), {}}}};
            if (!empty) value.items.front().mask_ids = {id(101U), id(duplicate ? 101U : 102U)};
            operation.payload = std::move(value);
            break;
        }
        default:
            break;
    }
    return operation;
}

TEST(PayloadStructure, EnforcesAllThirteenKeyedCollectionProfiles) {
    for (std::size_t profile = 0; profile < 13U; ++profile) {
        SCOPED_TRACE(profile);
        EXPECT_FALSE(validatePayloadStructure(collectionCase(profile, 0)).ok());
        EXPECT_FALSE(validatePayloadStructure(collectionCase(profile, 1)).ok());
        EXPECT_TRUE(validatePayloadStructure(collectionCase(profile, 2)).ok());
    }
}

TEST(PayloadStructure, EnforcesOperationWideNestedIdentityUniqueness) {
    Operation split;
    split.payload = SplitStrokesOp{{
        StrokeSplit{id(1U), {record(ObjectKind::kVectorStroke, 101U)}},
        StrokeSplit{id(2U), {record(ObjectKind::kVectorStroke, 101U)}},
    }};
    EXPECT_FALSE(validatePayloadStructure(split).ok());

    Operation erase;
    erase.payload = AddEraseMasksOp{{
        EraseMaskAddItem{id(1U), {mask(101U)}},
        EraseMaskAddItem{id(2U), {mask(101U)}},
    }};
    EXPECT_FALSE(validatePayloadStructure(erase).ok());
}

TEST(PayloadStructure, AcceptsOnlyReleasedObjectKindVersionContentTriples) {
    constexpr std::array kinds{
        ObjectKind::kShape, ObjectKind::kImage, ObjectKind::kVectorPath,
        ObjectKind::kRichText, ObjectKind::kVectorStroke, ObjectKind::kDabStroke,
        ObjectKind::kConnector, ObjectKind::kSticky, ObjectKind::kGroup};
    for (std::size_t index = 0; index < kinds.size(); ++index) {
        SCOPED_TRACE(index);
        Operation operation;
        operation.payload = AddStrokeOp{record(kinds[index], index + 1U)};
        EXPECT_TRUE(validatePayloadStructure(operation).ok());

        auto& object = std::get<AddStrokeOp>(operation.payload).object;
        object.kind_version = 0U;
        EXPECT_FALSE(validatePayloadStructure(operation).ok());
        object.kind_version = 2U;
        EXPECT_FALSE(validatePayloadStructure(operation).ok());
        object.kind_version = 1U;
        object.content = ShapeContent{1U, 1.0, 1.0};
        if (kinds[index] == ObjectKind::kShape) object.content = ImageContent{};
        EXPECT_FALSE(validatePayloadStructure(operation).ok());
    }

    Operation operation;
    operation.payload = AddStrokeOp{record(ObjectKind::kShape, 1U)};
    auto& object = std::get<AddStrokeOp>(operation.payload).object;
    object.kind = static_cast<ObjectKind>(0U);
    EXPECT_FALSE(validatePayloadStructure(operation).ok());
    object.kind = static_cast<ObjectKind>(10U);
    EXPECT_FALSE(validatePayloadStructure(operation).ok());
}

Operation patchOperation(PropertyPatch patch) {
    Operation operation;
    operation.payload = PatchPropertiesOp{{std::move(patch)}};
    return operation;
}

TEST(PayloadStructure, EnforcesPropertyPatchPresenceFieldAndValueType) {
    EXPECT_TRUE(validatePayloadStructure(patchOperation(
        {id(1U), 1U, PropertyPatchAction::kSet, PropertyValue{true}})).ok());
    EXPECT_TRUE(validatePayloadStructure(patchOperation(
        {id(1U), 1U, PropertyPatchAction::kClear, std::monostate{}})).ok());

    EXPECT_FALSE(validatePayloadStructure(patchOperation(
        {id(1U), 1U, PropertyPatchAction::kSet, std::monostate{}})).ok());
    EXPECT_FALSE(validatePayloadStructure(patchOperation(
        {id(1U), 1U, PropertyPatchAction::kClear, PropertyValue{true}})).ok());
    EXPECT_FALSE(validatePayloadStructure(patchOperation(
        {id(1U), 1U, PropertyPatchAction::kInvalid, std::monostate{}})).ok());
    EXPECT_FALSE(validatePayloadStructure(patchOperation(
        {id(1U), 0U, PropertyPatchAction::kSet, PropertyValue{true}})).ok());
    EXPECT_FALSE(validatePayloadStructure(patchOperation(
        {id(1U), 999U, PropertyPatchAction::kSet, PropertyValue{true}})).ok());
    EXPECT_FALSE(validatePayloadStructure(patchOperation(
        {id(1U), 1U, PropertyPatchAction::kSet, PropertyValue{1.0F}})).ok());
    EXPECT_FALSE(validatePayloadStructure(patchOperation(
        {id(1U), 3U, PropertyPatchAction::kSet, PropertyValue{1.25F}})).ok());
}

} // namespace
} // namespace canvas::semantic
