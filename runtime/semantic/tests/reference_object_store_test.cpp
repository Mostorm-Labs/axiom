#include "canvas/semantic/reference_object_store.hpp"

#include "object_store_mutator.hpp"

#include <gtest/gtest.h>

#include <optional>
#include <vector>

namespace canvas::semantic {

namespace {

ObjectRecord makeRecord(
    std::uint64_t id,
    ObjectKind kind,
    std::optional<ObjectId> parent,
    std::vector<std::uint8_t> order_key) {
    ObjectRecord record{};
    record.id = ObjectId::fromUint64(id);
    record.kind = kind;
    record.kind_version = 1U;
    record.placement = Placement{parent, OrderKey(std::move(order_key))};
    record.content = ObjectContent{kind, SemanticValue{{static_cast<std::uint8_t>(id)}}};
    return record;
}

} // namespace

TEST(ReferenceObjectStore, StartsEmptyAndDoesNotFindUnknownIdentity) {
    ReferenceObjectStore store;

    EXPECT_EQ(store.size(), 0U);
    EXPECT_FALSE(store.contains(ObjectId::fromUint64(99U)));
    EXPECT_EQ(store.find(ObjectId::fromUint64(99U)), nullptr);
    EXPECT_TRUE(store.allObjects().empty());
}

TEST(ReferenceObjectStore, StoresCompleteRecordsAndEnumeratesObjectIdsDeterministically) {
    ReferenceObjectStore store;
    const ObjectRecord second = makeRecord(2U, ObjectKind::kSticky, std::nullopt, {0x20U});
    const ObjectRecord first = makeRecord(1U, ObjectKind::kShape, std::nullopt, {0x10U});

    ASSERT_TRUE(internal::ObjectStoreMutator::insertFresh(store, second));
    ASSERT_TRUE(internal::ObjectStoreMutator::insertFresh(store, first));

    ASSERT_NE(store.find(first.id), nullptr);
    EXPECT_EQ(*store.find(first.id), first);
    ASSERT_EQ(store.allObjects().size(), 2U);
    EXPECT_EQ(store.allObjects()[0].id, first.id);
    EXPECT_EQ(store.allObjects()[1].id, second.id);
}

TEST(ReferenceObjectStore, TraversesRootAndChildrenByPlacementOrder) {
    ReferenceObjectStore store;
    const ObjectRecord parent = makeRecord(10U, ObjectKind::kGroup, std::nullopt, {0x01U});
    const ObjectRecord child_later = makeRecord(12U, ObjectKind::kShape, parent.id, {0x20U});
    const ObjectRecord child_earlier = makeRecord(11U, ObjectKind::kShape, parent.id, {0x10U});
    const ObjectRecord root = makeRecord(20U, ObjectKind::kSticky, std::nullopt, {0x30U});

    ASSERT_TRUE(internal::ObjectStoreMutator::insertFresh(store, child_later));
    ASSERT_TRUE(internal::ObjectStoreMutator::insertFresh(store, root));
    ASSERT_TRUE(internal::ObjectStoreMutator::insertFresh(store, parent));
    ASSERT_TRUE(internal::ObjectStoreMutator::insertFresh(store, child_earlier));

    const auto roots = store.children(std::nullopt);
    ASSERT_EQ(roots.size(), 2U);
    EXPECT_EQ(roots[0].id, parent.id);
    EXPECT_EQ(roots[1].id, root.id);

    const auto children = store.children(parent.id);
    ASSERT_EQ(children.size(), 2U);
    EXPECT_EQ(children[0].id, child_earlier.id);
    EXPECT_EQ(children[1].id, child_later.id);
}

} // namespace canvas::semantic
