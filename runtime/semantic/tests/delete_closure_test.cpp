#include "canvas/semantic/delete_closure.hpp"

#include "canvas/semantic/indexed_object_store.hpp"
#include "canvas/semantic/reference_object_store.hpp"
#include "canvas/semantic/staged_object_view.hpp"
#include "object_store_mutator.hpp"

#include <gtest/gtest.h>

#include <optional>
#include <span>
#include <vector>

namespace canvas::semantic {
namespace {

ObjectId id(std::uint64_t value) { return ObjectId::fromUint64(value); }

ObjectRecord shape(std::uint64_t value, std::optional<ObjectId> parent = std::nullopt) {
    ObjectRecord record{};
    record.id = id(value);
    record.kind = ObjectKind::kShape;
    record.kind_version = 1U;
    record.placement.parent_id = parent;
    record.placement.order_key = OrderKey({static_cast<std::uint8_t>(value)});
    record.content = ShapeContent{1U, 10.0, 10.0};
    return record;
}

ObjectRecord connector(std::uint64_t value, std::optional<ObjectId> start,
                       std::optional<ObjectId> end = std::nullopt) {
    ObjectRecord record{};
    record.id = id(value);
    record.kind = ObjectKind::kConnector;
    record.kind_version = 1U;
    record.placement.order_key = OrderKey({static_cast<std::uint8_t>(value)});
    ConnectorContent content{};
    if (start.has_value()) content.start.value = AttachedEndpoint{*start, AutoPerimeterAnchor{}};
    if (end.has_value()) content.end.value = AttachedEndpoint{*end, AutoPerimeterAnchor{}};
    record.content = content;
    return record;
}

template <typename Store>
void insert(Store& store, const ObjectRecord& record) {
    ASSERT_TRUE(internal::ObjectStoreMutator::insertFresh(store, record));
}

TEST(DeleteClosure, DirectTargetAndMultiWaveIndependentOracle) {
    ReferenceObjectStore base;
    insert(base, shape(1));
    insert(base, connector(10, id(1)));
    insert(base, shape(2, id(10)));
    insert(base, connector(20, id(2)));
    StagedObjectView staged(base);

    DeleteClosure closure{};
    const ObjectId requested[] = {id(1)};
    ASSERT_TRUE(resolveDeleteClosure(staged, std::span<const ObjectId>(requested), &closure).ok());
    EXPECT_EQ(closure.requested_delete_ids, std::vector<ObjectId>({id(1)}));
    EXPECT_EQ(closure.resolved_hierarchy_closure, std::vector<ObjectId>({id(2)}));
    EXPECT_EQ(closure.resolved_connector_cascade_closure,
              std::vector<ObjectId>({id(10), id(20)}));
    EXPECT_EQ(closure.final_delete_set,
              std::vector<ObjectId>({id(1), id(2), id(10), id(20)}));
}

TEST(DeleteClosure, MissingIsAtomicAndFreePointUnrelated) {
    ReferenceObjectStore base;
    insert(base, shape(1));
    insert(base, connector(10, std::nullopt, std::nullopt));
    StagedObjectView staged(base);
    DeleteClosure closure{};
    closure.final_delete_set = {id(99)};
    const DeleteClosure before = closure;
    const ObjectId requested[] = {id(1), id(99)};
    const StatefulResult result =
        resolveDeleteClosure(staged, std::span<const ObjectId>(requested), &closure);
    EXPECT_EQ(result.issue, StatefulIssue::kObjectMissing);
    EXPECT_EQ(closure.final_delete_set, before.final_delete_set);
}

TEST(DeleteClosure, StagedReplacementAndIndexedParity) {
    ReferenceObjectStore reference;
    IndexedObjectStore indexed;
    const ObjectRecord a = shape(1);
    const ObjectRecord c = connector(10, id(1));
    insert(reference, a); insert(reference, c);
    insert(indexed, a); insert(indexed, c);
    StagedObjectView ref_staged(reference);
    StagedObjectView idx_staged(indexed);
    ObjectRecord replacement = c;
    replacement.content = connector(10, std::nullopt, std::nullopt).content;
    ASSERT_TRUE(ref_staged.stageReplace(replacement));
    ASSERT_TRUE(idx_staged.stageReplace(replacement));
    const ObjectId requested[] = {id(1)};
    DeleteClosure ref{}, idx{};
    ASSERT_TRUE(resolveDeleteClosure(ref_staged, requested, &ref).ok());
    ASSERT_TRUE(resolveDeleteClosure(idx_staged, requested, &idx).ok());
    EXPECT_EQ(ref.final_delete_set, idx.final_delete_set);
    EXPECT_EQ(ref.final_delete_set, std::vector<ObjectId>({id(1)}));
}

} // namespace
} // namespace canvas::semantic
