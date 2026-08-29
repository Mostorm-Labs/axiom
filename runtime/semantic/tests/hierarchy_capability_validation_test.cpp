#include "canvas/semantic/hierarchy_capability_validation.hpp"

#include "canvas/semantic/indexed_object_store.hpp"
#include "canvas/semantic/reference_object_store.hpp"
#include "object_store_mutator.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace canvas::semantic {
namespace {

ObjectId id(std::uint64_t value) { return ObjectId::fromUint64(value); }

ObjectRecord record(
    std::uint64_t value,
    ObjectKind kind,
    std::optional<ObjectId> parent = std::nullopt) {
    ObjectRecord result{};
    result.id = id(value);
    result.kind = kind;
    result.kind_version = 1U;
    result.placement = Placement{parent, OrderKey({static_cast<std::uint8_t>(value)})};
    switch (kind) {
        case ObjectKind::kShape:
            result.content = ShapeContent{1U, 10.0, 10.0};
            break;
        case ObjectKind::kSticky:
            result.content = StickyContent{10.0, 10.0};
            break;
        case ObjectKind::kRichText:
            result.content = RichTextContent{};
            break;
        case ObjectKind::kGroup:
            result.content = GroupContent{};
            break;
        default:
            result.content = ShapeContent{1U, 10.0, 10.0};
            break;
    }
    return result;
}

ObjectRecord shape(std::uint64_t value, std::optional<ObjectId> parent = std::nullopt) {
    return record(value, ObjectKind::kShape, parent);
}
ObjectRecord group(std::uint64_t value, std::optional<ObjectId> parent = std::nullopt) {
    return record(value, ObjectKind::kGroup, parent);
}
ObjectRecord sticky(std::uint64_t value, std::optional<ObjectId> parent = std::nullopt) {
    return record(value, ObjectKind::kSticky, parent);
}
ObjectRecord richText(std::uint64_t value, std::optional<ObjectId> parent = std::nullopt) {
    return record(value, ObjectKind::kRichText, parent);
}

template <typename Store>
void insert(Store& store, const ObjectRecord& value) {
    ASSERT_TRUE(internal::ObjectStoreMutator::insertFresh(store, value));
}

StatefulResult validate(
    StagedObjectView& staged,
    std::initializer_list<ObjectId> ids) {
    return validateStagedHierarchyCapabilities(
        staged, std::span<const ObjectId>(ids.begin(), ids.size()));
}

TEST(HierarchyCapabilityValidation, HCV_B01_RootShapeAccepts) {
    ReferenceObjectStore base;
    StagedObjectView staged(base);
    ASSERT_TRUE(staged.stageCreate(shape(1U)));
    EXPECT_TRUE(validate(staged, {id(1U)}).ok());
}

TEST(HierarchyCapabilityValidation, HCV_B02_GroupShapeAccepts) {
    ReferenceObjectStore base;
    StagedObjectView staged(base);
    ASSERT_TRUE(staged.stageCreate(group(1U)));
    ASSERT_TRUE(staged.stageCreate(shape(2U, id(1U))));
    EXPECT_TRUE(validate(staged, {id(2U)}).ok());
}

TEST(HierarchyCapabilityValidation, HCV_B03_GroupGroupAccepts) {
    ReferenceObjectStore base;
    StagedObjectView staged(base);
    ASSERT_TRUE(staged.stageCreate(group(1U)));
    ASSERT_TRUE(staged.stageCreate(group(2U, id(1U))));
    EXPECT_TRUE(validate(staged, {id(2U)}).ok());
}

TEST(HierarchyCapabilityValidation, HCV_B04_GroupStickyAccepts) {
    ReferenceObjectStore base;
    StagedObjectView staged(base);
    ASSERT_TRUE(staged.stageCreate(group(1U)));
    ASSERT_TRUE(staged.stageCreate(sticky(2U, id(1U))));
    EXPECT_TRUE(validate(staged, {id(2U)}).ok());
}

TEST(HierarchyCapabilityValidation, HCV_B05_StickyRichTextAccepts) {
    ReferenceObjectStore base;
    StagedObjectView staged(base);
    ASSERT_TRUE(staged.stageCreate(sticky(1U)));
    ASSERT_TRUE(staged.stageCreate(richText(2U, id(1U))));
    EXPECT_TRUE(validate(staged, {id(2U)}).ok());
}

TEST(HierarchyCapabilityValidation, HCV_B06_EmptyStickyAccepts) {
    ReferenceObjectStore base;
    StagedObjectView staged(base);
    ASSERT_TRUE(staged.stageCreate(sticky(1U)));
    EXPECT_TRUE(validate(staged, {id(1U)}).ok());
}

TEST(HierarchyCapabilityValidation, HCV_B07_StickyShapeRejects) {
    ReferenceObjectStore base;
    StagedObjectView staged(base);
    ASSERT_TRUE(staged.stageCreate(sticky(1U)));
    ASSERT_TRUE(staged.stageCreate(shape(2U, id(1U))));
    EXPECT_EQ(validate(staged, {id(2U)}).issue, StatefulIssue::kInvalidApplicability);
}

TEST(HierarchyCapabilityValidation, HCV_B08_ShapeRichTextRejects) {
    ReferenceObjectStore base;
    StagedObjectView staged(base);
    ASSERT_TRUE(staged.stageCreate(shape(1U)));
    ASSERT_TRUE(staged.stageCreate(richText(2U, id(1U))));
    EXPECT_EQ(validate(staged, {id(2U)}).issue, StatefulIssue::kInvalidApplicability);
}

TEST(HierarchyCapabilityValidation, HCV_B09_ConnectorShapeRejects) {
    ReferenceObjectStore base;
    StagedObjectView staged(base);
    ObjectRecord connector = record(1U, ObjectKind::kConnector);
    ASSERT_TRUE(staged.stageCreate(connector));
    ASSERT_TRUE(staged.stageCreate(shape(2U, id(1U))));
    EXPECT_EQ(validate(staged, {id(2U)}).issue, StatefulIssue::kInvalidApplicability);
}

TEST(HierarchyCapabilityValidation, HCV_B10_SecondStickyRichTextRejects) {
    ReferenceObjectStore base;
    StagedObjectView staged(base);
    ASSERT_TRUE(staged.stageCreate(sticky(1U)));
    ASSERT_TRUE(staged.stageCreate(richText(2U, id(1U))));
    ASSERT_TRUE(staged.stageCreate(richText(3U, id(1U))));
    EXPECT_EQ(validate(staged, {id(2U), id(3U)}).issue, StatefulIssue::kInvalidApplicability);
}

TEST(HierarchyCapabilityValidation, HCV_B11_OrderingDoesNotChangeResult) {
    ReferenceObjectStore base;
    StagedObjectView staged(base);
    ASSERT_TRUE(staged.stageCreate(sticky(9U)));
    ASSERT_TRUE(staged.stageCreate(richText(1U, id(9U))));
    ASSERT_TRUE(validate(staged, {id(1U), id(9U)}).ok());
    EXPECT_TRUE(validate(staged, {id(9U), id(1U)}).ok());
}

TEST(HierarchyCapabilityValidation, HCV_B12_ExistingGroupParentAcceptsStagedChild) {
    ReferenceObjectStore base;
    insert(base, group(1U));
    StagedObjectView staged(base);
    ASSERT_TRUE(staged.stageCreate(shape(2U, id(1U))));
    EXPECT_TRUE(validate(staged, {id(2U)}).ok());
}

TEST(HierarchyCapabilityValidation, HCV_B13_ExistingEmptyStickyAcceptsStagedRichText) {
    ReferenceObjectStore base;
    insert(base, sticky(1U));
    StagedObjectView staged(base);
    ASSERT_TRUE(staged.stageCreate(richText(2U, id(1U))));
    EXPECT_TRUE(validate(staged, {id(2U)}).ok());
}

TEST(HierarchyCapabilityValidation, HCV_B14_ExistingStickySecondRichTextRejects) {
    ReferenceObjectStore base;
    insert(base, sticky(1U));
    insert(base, richText(2U, id(1U)));
    StagedObjectView staged(base);
    ASSERT_TRUE(staged.stageCreate(richText(3U, id(1U))));
    EXPECT_EQ(validate(staged, {id(3U)}).issue, StatefulIssue::kInvalidApplicability);
}

TEST(HierarchyCapabilityValidation, HCV_B15_ReferenceIndexedParityAndNoMutation) {
    ReferenceObjectStore reference;
    IndexedObjectStore indexed;
    insert(reference, sticky(1U));
    insert(reference, richText(2U, id(1U)));
    insert(indexed, sticky(1U));
    insert(indexed, richText(2U, id(1U)));
    ASSERT_TRUE(reference.allObjects() == indexed.allObjects());
    const auto before = reference.allObjects();
    const auto indexed_before = indexed.allObjects();
    StagedObjectView reference_staged(reference);
    StagedObjectView indexed_staged(indexed);
    ASSERT_TRUE(reference_staged.stageCreate(richText(3U, id(1U))));
    ASSERT_TRUE(indexed_staged.stageCreate(richText(3U, id(1U))));
    EXPECT_EQ(validate(reference_staged, {id(3U)}).issue,
              validate(indexed_staged, {id(3U)}).issue);
    EXPECT_EQ(reference.allObjects(), before);
    EXPECT_EQ(indexed.allObjects(), indexed_before);
    EXPECT_TRUE(internal::ObjectStoreMutator::indexMatchesRebuild(indexed));
}

TEST(HierarchyCapabilityValidation, HCV_B16_NoAllObjectsAndOverlayLookups) {
    class CountingStore final : public ObjectStore {
      public:
        explicit CountingStore(const ObjectStore& delegate) : delegate_(delegate) {}
        std::size_t size() const noexcept override { return delegate_.size(); }
        bool contains(const ObjectId& value) const noexcept override {
            ++contains_calls;
            return delegate_.contains(value);
        }
        const ObjectRecord* find(const ObjectId& value) const noexcept override {
            ++find_calls;
            return delegate_.find(value);
        }
        std::vector<ObjectRecord> allObjects() const override {
            ++all_objects_calls;
            return delegate_.allObjects();
        }
        std::vector<ObjectRecord> children(const std::optional<ObjectId>& parent) const override {
            ++children_calls;
            return delegate_.children(parent);
        }
        mutable std::size_t contains_calls = 0U;
        mutable std::size_t find_calls = 0U;
        mutable std::size_t all_objects_calls = 0U;
        mutable std::size_t children_calls = 0U;
      private:
        const ObjectStore& delegate_;
    };
    ReferenceObjectStore base;
    CountingStore counting(base);
    StagedObjectView staged(counting);
    ASSERT_TRUE(staged.stageCreate(group(1U)));
    ASSERT_TRUE(staged.stageCreate(shape(2U, id(1U))));
    EXPECT_TRUE(validate(staged, {id(2U)}).ok());
    EXPECT_EQ(counting.all_objects_calls, 0U);
    EXPECT_EQ(counting.children_calls, 0U);
    ASSERT_TRUE(staged.stageCreate(sticky(3U)));
    ASSERT_TRUE(staged.stageCreate(richText(4U, id(3U))));
    EXPECT_TRUE(validate(staged, {id(4U)}).ok());
    EXPECT_EQ(counting.all_objects_calls, 0U);
    EXPECT_EQ(counting.children_calls, 1U);
}

} // namespace
} // namespace canvas::semantic
