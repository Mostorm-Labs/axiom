#include "canvas/semantic/indexed_object_store.hpp"
#include "canvas/semantic/reference_object_store.hpp"
#include "canvas/semantic/staged_object_view.hpp"

#include "object_store_mutator.hpp"

#include <gtest/gtest.h>

#include <optional>
#include <vector>

namespace canvas::semantic {
namespace {

ObjectRecord record(std::uint64_t id, std::optional<ObjectId> parent = std::nullopt,
                    std::vector<std::uint8_t> order = {1U}) {
    ObjectRecord value{};
    value.id = ObjectId::fromUint64(id);
    value.kind = ObjectKind::kShape;
    value.kind_version = 1U;
    value.placement = Placement{parent, OrderKey(std::move(order))};
    value.content = ShapeContent{7U, static_cast<double>(id), 20.0};
    return value;
}

template <typename Store>
void insert(Store& store, const ObjectRecord& value) {
    ASSERT_TRUE(internal::ObjectStoreMutator::insertFresh(store, value));
}

class CountingObjectStore final : public ObjectStore {
  public:
    explicit CountingObjectStore(const ObjectStore& delegate) : delegate_(delegate) {}

    [[nodiscard]] std::size_t size() const noexcept override { return delegate_.size(); }
    [[nodiscard]] bool contains(const ObjectId& id) const noexcept override {
        ++contains_calls;
        return delegate_.contains(id);
    }
    [[nodiscard]] const ObjectRecord* find(const ObjectId& id) const noexcept override {
        ++find_calls;
        return delegate_.find(id);
    }
    [[nodiscard]] std::vector<ObjectRecord> allObjects() const override {
        ++all_objects_calls;
        return delegate_.allObjects();
    }
    [[nodiscard]] std::vector<ObjectRecord> children(
        const std::optional<ObjectId>& parent_id) const override {
        ++children_calls;
        return delegate_.children(parent_id);
    }

    void reset() const noexcept {
        find_calls = 0U;
        contains_calls = 0U;
        children_calls = 0U;
        all_objects_calls = 0U;
    }

    mutable std::size_t find_calls = 0U;
    mutable std::size_t contains_calls = 0U;
    mutable std::size_t children_calls = 0U;
    mutable std::size_t all_objects_calls = 0U;

  private:
    const ObjectStore& delegate_;
};

} // namespace

TEST(StagedObjectView, CreateIsVisibleOnlyThroughOverlay) {
    ReferenceObjectStore base;
    const ObjectRecord created = record(2U);
    StagedObjectView overlay(base);

    EXPECT_FALSE(base.contains(created.id));
    ASSERT_TRUE(overlay.stageCreate(created));
    EXPECT_TRUE(overlay.contains(created.id));
    ASSERT_NE(overlay.find(created.id), nullptr);
    EXPECT_EQ(*overlay.find(created.id), created);
    EXPECT_FALSE(base.contains(created.id));
    EXPECT_EQ(base.find(created.id), nullptr);
}

TEST(StagedObjectView, DeleteIsVisibleOnlyThroughOverlay) {
    ReferenceObjectStore base;
    const ObjectRecord existing = record(3U);
    insert(base, existing);
    StagedObjectView overlay(base);

    ASSERT_TRUE(overlay.stageDelete(existing.id));
    EXPECT_FALSE(overlay.contains(existing.id));
    EXPECT_EQ(overlay.find(existing.id), nullptr);
    EXPECT_TRUE(base.contains(existing.id));
    ASSERT_NE(base.find(existing.id), nullptr);
    EXPECT_EQ(*base.find(existing.id), existing);
}

TEST(StagedObjectView, ReplaceIsVisibleOnlyThroughOverlay) {
    ReferenceObjectStore base;
    const ObjectRecord original = record(4U);
    ObjectRecord replacement = original;
    replacement.transform.tx = 42.0;
    insert(base, original);
    StagedObjectView overlay(base);

    ASSERT_TRUE(overlay.stageReplace(replacement));
    ASSERT_NE(overlay.find(original.id), nullptr);
    EXPECT_EQ(*overlay.find(original.id), replacement);
    ASSERT_NE(base.find(original.id), nullptr);
    EXPECT_EQ(*base.find(original.id), original);
}

TEST(StagedObjectView, ProjectionAndChildrenAreDeterministic) {
    ReferenceObjectStore first;
    ReferenceObjectStore second;
    const ObjectRecord parent = record(10U, std::nullopt, {0x10U});
    const ObjectRecord child_a = record(12U, parent.id, {0x20U});
    const ObjectRecord child_b = record(11U, parent.id, {0x20U});
    insert(first, child_a);
    insert(first, parent);
    insert(first, child_b);
    insert(second, child_b);
    insert(second, child_a);
    insert(second, parent);

    StagedObjectView first_overlay(first);
    StagedObjectView second_overlay(second);
    ASSERT_TRUE(first_overlay.stageCreate(record(20U, parent.id, {0x01U})));
    ASSERT_TRUE(first_overlay.stageDelete(child_a.id));
    ASSERT_TRUE(second_overlay.stageDelete(child_a.id));
    ASSERT_TRUE(second_overlay.stageCreate(record(20U, parent.id, {0x01U})));

    EXPECT_EQ(first_overlay.projection(), second_overlay.projection());
    EXPECT_EQ(first_overlay.children(parent.id), second_overlay.children(parent.id));
    ASSERT_EQ(first_overlay.children(parent.id).size(), 2U);
    EXPECT_EQ(first_overlay.children(parent.id)[0].id, ObjectId::fromUint64(20U));
    EXPECT_EQ(first_overlay.children(parent.id)[1].id, child_b.id);
}

TEST(StagedObjectView, ReferenceAndIndexedOverlaysHaveEqualProjections) {
    ReferenceObjectStore reference;
    IndexedObjectStore indexed;
    const ObjectRecord parent = record(30U, std::nullopt, {0x10U});
    const ObjectRecord child = record(31U, parent.id, {0x20U});
    insert(reference, parent);
    insert(reference, child);
    insert(indexed, parent);
    insert(indexed, child);

    StagedObjectView reference_overlay(reference);
    StagedObjectView indexed_overlay(indexed);
    ObjectRecord replacement = child;
    replacement.transform.ty = 99.0;
    ASSERT_TRUE(reference_overlay.stageReplace(replacement));
    ASSERT_TRUE(indexed_overlay.stageReplace(replacement));
    ASSERT_TRUE(reference_overlay.stageCreate(record(32U, parent.id, {0x30U})));
    ASSERT_TRUE(indexed_overlay.stageCreate(record(32U, parent.id, {0x30U})));

    EXPECT_EQ(reference_overlay.projection(), indexed_overlay.projection());
    EXPECT_EQ(reference_overlay.children(parent.id), indexed_overlay.children(parent.id));
}

TEST(StagedObjectView, StagingNeverMutatesBaseStoresOrObjectIndex) {
    ReferenceObjectStore reference;
    IndexedObjectStore indexed;
    const ObjectRecord existing = record(40U, std::nullopt, {0x10U});
    const ObjectRecord deleted = record(41U, std::nullopt, {0x20U});
    insert(reference, existing);
    insert(reference, deleted);
    insert(indexed, existing);
    insert(indexed, deleted);
    const auto reference_before = reference.allObjects();
    const auto indexed_before = indexed.allObjects();

    StagedObjectView reference_overlay(reference);
    StagedObjectView indexed_overlay(indexed);
    ObjectRecord replacement = existing;
    replacement.transform.tx = 5.0;
    for (StagedObjectView* overlay : {&reference_overlay, &indexed_overlay}) {
        ASSERT_TRUE(overlay->stageReplace(replacement));
        ASSERT_TRUE(overlay->stageDelete(deleted.id));
        ASSERT_TRUE(overlay->stageCreate(record(42U, std::nullopt, {0x30U})));
    }

    EXPECT_EQ(reference.allObjects(), reference_before);
    EXPECT_EQ(indexed.allObjects(), indexed_before);
    EXPECT_TRUE(internal::ObjectStoreMutator::indexMatchesRebuild(indexed));
}

TEST(StagedObjectView, MovedReplacementIsReconciledAcrossParentChildren) {
    ReferenceObjectStore base;
    const ObjectRecord parent_p = record(50U, std::nullopt, {0x10U});
    const ObjectRecord parent_q = record(51U, std::nullopt, {0x20U});
    const ObjectRecord child = record(52U, parent_p.id, {0x10U});
    insert(base, parent_p);
    insert(base, parent_q);
    insert(base, child);

    StagedObjectView overlay(base);
    ObjectRecord moved = child;
    moved.placement = Placement{parent_q.id, OrderKey({0x01U})};
    ASSERT_TRUE(overlay.stageReplace(moved));

    EXPECT_TRUE(overlay.children(parent_p.id).empty());
    ASSERT_EQ(overlay.children(parent_q.id).size(), 1U);
    EXPECT_EQ(overlay.children(parent_q.id).front(), moved);
}

TEST(StagedObjectView, ChildrenUsesIndexedBaseLookupWithoutAllObjectsScan) {
    ReferenceObjectStore base;
    const ObjectRecord parent = record(60U, std::nullopt, {0x10U});
    const ObjectRecord child = record(61U, parent.id, {0x10U});
    insert(base, parent);
    insert(base, child);
    CountingObjectStore counting(base);
    StagedObjectView overlay(counting);

    counting.reset();
    const auto children = overlay.children(parent.id);
    ASSERT_EQ(children.size(), 1U);
    EXPECT_EQ(counting.children_calls, 1U);
    EXPECT_EQ(counting.all_objects_calls, 0U);
}

TEST(StagedObjectView, FindAndContainsDoNotEnumerateBaseObjects) {
    ReferenceObjectStore base;
    const ObjectRecord existing = record(70U);
    insert(base, existing);
    CountingObjectStore counting(base);
    StagedObjectView overlay(counting);

    counting.reset();
    ASSERT_NE(overlay.find(existing.id), nullptr);
    EXPECT_EQ(counting.all_objects_calls, 0U);
    counting.reset();
    EXPECT_TRUE(overlay.contains(existing.id));
    EXPECT_EQ(counting.all_objects_calls, 0U);
}

TEST(StagedObjectView, MovedReplacementChildrenAvoidAllObjectsScan) {
    ReferenceObjectStore base;
    const ObjectRecord parent_p = record(80U, std::nullopt, {0x10U});
    const ObjectRecord parent_q = record(81U, std::nullopt, {0x20U});
    const ObjectRecord child = record(82U, parent_p.id, {0x10U});
    insert(base, parent_p);
    insert(base, parent_q);
    insert(base, child);
    CountingObjectStore counting(base);
    StagedObjectView overlay(counting);
    ObjectRecord moved = child;
    moved.placement = Placement{parent_q.id, OrderKey({0x01U})};
    ASSERT_TRUE(overlay.stageReplace(moved));

    counting.reset();
    EXPECT_TRUE(overlay.children(parent_p.id).empty());
    EXPECT_EQ(counting.all_objects_calls, 0U);
    counting.reset();
    ASSERT_EQ(overlay.children(parent_q.id).size(), 1U);
    EXPECT_EQ(counting.all_objects_calls, 0U);
}

} // namespace canvas::semantic
