#include "canvas/semantic/normalizer.hpp"

#include <gtest/gtest.h>

#include <cmath>

namespace canvas::semantic {
namespace {

ObjectId id(std::uint64_t value) { return ObjectId::fromUint64(value); }

Operation deleteOperation(std::initializer_list<std::uint64_t> values) {
    Operation operation;
    DeleteObjectsOp payload;
    for (const auto value : values) payload.object_ids.push_back(id(value));
    operation.payload = std::move(payload);
    return operation;
}

TEST(OperationNormalizer, SortsDeleteIdsAndRejectsDuplicateIds) {
    const auto normalized = normalizeOperation(deleteOperation({9U, 2U}));
    ASSERT_TRUE(normalized.ok());
    const auto& ids = std::get<DeleteObjectsOp>(normalized.value.payload).object_ids;
    ASSERT_EQ(ids.size(), 2U);
    EXPECT_EQ(ids[0], id(2U));
    EXPECT_EQ(ids[1], id(9U));

    EXPECT_FALSE(normalizeOperation(deleteOperation({2U, 2U})).ok());
}

TEST(OperationNormalizer, NormalizesNegativeZeroWithoutMutatingInput) {
    Operation operation;
    operation.payload = SetTransformsOp{{TransformItem{id(7U), Transform2D{-0.0, -0.0, 1.0, 1.0, -0.0, 2.0}}}};
    const auto normalized = normalizeOperation(operation);
    ASSERT_TRUE(normalized.ok());
    const auto& result = std::get<SetTransformsOp>(normalized.value.payload).items.front().transform;
    EXPECT_FALSE(std::signbit(result.a));
    EXPECT_FALSE(std::signbit(result.b));
    EXPECT_FALSE(std::signbit(result.tx));
    EXPECT_TRUE(std::signbit(std::get<SetTransformsOp>(operation.payload).items.front().transform.a));
}

TEST(OperationNormalizer, SortsPropertyBagAndRejectsDuplicateFieldIds) {
    Operation operation;
    ObjectRecord record;
    record.id = id(11U);
    record.properties.entries = {{7U, PropertyValue{true}}, {2U, PropertyValue{false}}};
    InsertObjectsOp payload;
    payload.objects.push_back(record);
    operation.payload = std::move(payload);

    const auto normalized = normalizeOperation(operation);
    ASSERT_TRUE(normalized.ok());
    const auto& entries = std::get<InsertObjectsOp>(normalized.value.payload).objects.front().properties.entries;
    ASSERT_EQ(entries.size(), 2U);
    EXPECT_EQ(entries[0].field_id, 2U);
    EXPECT_EQ(entries[1].field_id, 7U);

    std::get<InsertObjectsOp>(operation.payload).objects.front().properties.entries.push_back({2U, PropertyValue{true}});
    EXPECT_FALSE(normalizeOperation(operation).ok());
}

TEST(OperationNormalizer, EnforcesOperationWideSplitReplacementUniqueness) {
    Operation operation;
    operation.payload = SplitStrokesOp{{
        StrokeSplit{id(1U), {ObjectRecord{.id = id(10U)} }},
        StrokeSplit{id(2U), {ObjectRecord{.id = id(10U)} }},
    }};
    EXPECT_FALSE(normalizeOperation(operation).ok());
}

TEST(OperationNormalizer, SortsNestedEraseMasksAndRejectsEmptyKeyedBatches) {
    Operation operation;
    operation.payload = AddEraseMasksOp{{EraseMaskAddItem{
        id(4U),
        {EraseMaskRecord{id(9U)}, EraseMaskRecord{id(3U)}},
    }}};
    const auto normalized = normalizeOperation(operation);
    ASSERT_TRUE(normalized.ok());
    const auto& masks = std::get<AddEraseMasksOp>(normalized.value.payload).items.front().masks;
    EXPECT_EQ(masks[0].id, id(3U));
    EXPECT_EQ(masks[1].id, id(9U));

    EXPECT_FALSE(normalizeOperation(Operation{.payload = AddEraseMasksOp{}}).ok());
}

TEST(OperationNormalizer, PreservesOrderedLeafSequences) {
    Operation operation;
    const ObjectId paragraph = id(8U);
    RichTextDelta delta;
    delta.delta_version = 1U;
    delta.steps = {DeleteTextStep{paragraph, 9U, 1U}, DeleteTextStep{paragraph, 2U, 3U}};
    operation.payload = EditRichTextOp{id(5U), std::move(delta)};

    const auto normalized = normalizeOperation(operation);
    ASSERT_TRUE(normalized.ok());
    const auto& steps = std::get<EditRichTextOp>(normalized.value.payload).delta.steps;
    ASSERT_EQ(steps.size(), 2U);
    EXPECT_EQ(std::get<DeleteTextStep>(steps[0]).start_scalar, 9U);
    EXPECT_EQ(std::get<DeleteTextStep>(steps[1]).start_scalar, 2U);
}

TEST(OperationNormalizer, ElidesFullImageCropWithoutChangingPartialCrop) {
    Operation operation;
    operation.payload = SetImageContentOp{
        id(5U), ImageContent{ResourceId{id(9U)}, 640.0, 480.0,
                              NormalizedRect{0.0, 0.0, 1.0, 1.0},
                              ImageContentMode::kFit, 320.0, 240.0}};

    const auto normalized = normalizeOperation(operation);
    ASSERT_TRUE(normalized.ok());
    const auto& content = std::get<SetImageContentOp>(normalized.value.payload).content;
    EXPECT_FALSE(content.source_rect.has_value());
    EXPECT_TRUE(std::get<SetImageContentOp>(operation.payload).content.source_rect.has_value());

    operation.payload = SetImageContentOp{
        id(5U), ImageContent{ResourceId{id(9U)}, 640.0, 480.0,
                              NormalizedRect{0.0, 0.0, 0.5, 1.0},
                              ImageContentMode::kFit, 320.0, 240.0}};
    const auto partial = normalizeOperation(operation);
    ASSERT_TRUE(partial.ok());
    EXPECT_TRUE(std::get<SetImageContentOp>(partial.value.payload).content.source_rect.has_value());
}

} // namespace
} // namespace canvas::semantic
