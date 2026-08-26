#include "canvas/semantic/indexed_object_store.hpp"

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
    std::vector<std::uint8_t> order_key,
    std::uint8_t content_tag = 1U) {
    ObjectRecord record{};
    record.id = ObjectId::fromUint64(id);
    record.kind = kind;
    record.kind_version = 1U;
    record.placement = Placement{parent, OrderKey(std::move(order_key))};
    record.content = ObjectContent{kind, SemanticValue{{content_tag}}};
    return record;
}

} // namespace

TEST(IndexedObjectStore, KeepsPrivateParentChildrenIndexSynchronizedWithRecords) {
    IndexedObjectStore store;
    const ObjectRecord first_parent = makeRecord(1U, ObjectKind::kGroup, std::nullopt, {0x10U});
    const ObjectRecord second_parent = makeRecord(2U, ObjectKind::kGroup, std::nullopt, {0x20U});
    const ObjectRecord child = makeRecord(3U, ObjectKind::kShape, first_parent.id, {0x30U});

    ASSERT_TRUE(internal::ObjectStoreMutator::insertFresh(store, first_parent));
    ASSERT_TRUE(internal::ObjectStoreMutator::insertFresh(store, second_parent));
    ASSERT_TRUE(internal::ObjectStoreMutator::insertFresh(store, child));
    EXPECT_TRUE(internal::ObjectStoreMutator::indexMatchesRebuild(store));

    auto children = store.children(first_parent.id);
    ASSERT_EQ(children.size(), 1U);
    EXPECT_EQ(children.front(), child);

    ObjectRecord reparented = child;
    reparented.placement = Placement{second_parent.id, OrderKey({0x05U})};
    reparented.content = ObjectContent{ObjectKind::kShape, SemanticValue{{0x99U}}};
    ASSERT_TRUE(internal::ObjectStoreMutator::replaceExisting(store, reparented));
    EXPECT_TRUE(store.children(first_parent.id).empty());
    children = store.children(second_parent.id);
    ASSERT_EQ(children.size(), 1U);
    EXPECT_EQ(children.front(), reparented);
    EXPECT_TRUE(internal::ObjectStoreMutator::indexMatchesRebuild(store));

    ASSERT_TRUE(internal::ObjectStoreMutator::eraseExisting(store, child.id));
    EXPECT_EQ(store.find(child.id), nullptr);
    EXPECT_TRUE(store.children(second_parent.id).empty());
    EXPECT_TRUE(internal::ObjectStoreMutator::indexMatchesRebuild(store));
}

TEST(IndexedObjectStore, ReportsOnlyInternalMutationPreconditionFailures) {
    IndexedObjectStore store;
    const ObjectRecord record = makeRecord(7U, ObjectKind::kSticky, std::nullopt, {0x10U});

    EXPECT_FALSE(internal::ObjectStoreMutator::replaceExisting(store, record));
    EXPECT_FALSE(internal::ObjectStoreMutator::eraseExisting(store, record.id));
    ASSERT_TRUE(internal::ObjectStoreMutator::insertFresh(store, record));
    EXPECT_FALSE(internal::ObjectStoreMutator::insertFresh(store, record));
    EXPECT_TRUE(internal::ObjectStoreMutator::indexMatchesRebuild(store));
}

} // namespace canvas::semantic
