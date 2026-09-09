#include "canvas/semantic/indexed_object_store.hpp"
#include "canvas/semantic/reference_object_store.hpp"
#include "canvas/semantic/object_record.hpp"
#include "../tools/g1_07_replay_corpus.hpp"
#include "../tools/g1_07_replay_inspector.hpp"

#include "object_store_mutator.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <optional>
#include <vector>

namespace canvas::semantic {
namespace {

ObjectRecord record(std::uint64_t value, std::optional<ObjectId> parent = std::nullopt,
                    std::uint8_t order = 1U) {
    ObjectRecord result{};
    result.id = ObjectId::fromUint64(value);
    result.kind = ObjectKind::kShape;
    result.kind_version = 1U;
    result.placement = Placement{parent, OrderKey({order})};
    result.transform = Transform2D{1.0, 0.0, 0.0, 1.0, static_cast<double>(value), 0.0};
    result.content = ShapeContent{1U, 10.0, 20.0};
    return result;
}

template <typename Store>
void populate(Store& store) {
    const auto parent = record(1U);
    ASSERT_TRUE(internal::ObjectStoreMutator::insertFresh(store, parent));
    ASSERT_TRUE(internal::ObjectStoreMutator::insertFresh(store, record(2U, parent.id, 2U)));
    ASSERT_TRUE(internal::ObjectStoreMutator::insertFresh(store, record(3U, parent.id, 1U)));
    ASSERT_TRUE(internal::ObjectStoreMutator::insertFresh(store, record(4U)));
}

template <typename Store>
std::vector<ObjectRecord> snapshot(const Store& store) {
    return store.allObjects();
}

} // namespace

TEST(G108ReferenceDifferential, IndexedMatchesReferenceForCanonicalRecordAndHierarchyViews) {
    ReferenceObjectStore reference;
    IndexedObjectStore indexed;
    populate(reference);
    populate(indexed);

    EXPECT_EQ(snapshot(reference), snapshot(indexed));
    EXPECT_EQ(reference.children(std::nullopt), indexed.children(std::nullopt));
    EXPECT_EQ(reference.children(ObjectId::fromUint64(1U)),
              indexed.children(ObjectId::fromUint64(1U)));

    ObjectRecord moved = record(3U, ObjectId::fromUint64(4U), 1U);
    ASSERT_TRUE(internal::ObjectStoreMutator::replaceExisting(reference, moved));
    ASSERT_TRUE(internal::ObjectStoreMutator::replaceExisting(indexed, moved));
    EXPECT_EQ(snapshot(reference), snapshot(indexed));
    EXPECT_EQ(reference.children(ObjectId::fromUint64(1U)),
              indexed.children(ObjectId::fromUint64(1U)));
    EXPECT_EQ(reference.children(ObjectId::fromUint64(4U)),
              indexed.children(ObjectId::fromUint64(4U)));
    EXPECT_TRUE(internal::ObjectStoreMutator::indexMatchesRebuild(indexed));
}

TEST(G108ReferenceDifferential, IndexedNegativeMutationLeavesCanonicalViewsEqual) {
    ReferenceObjectStore reference;
    IndexedObjectStore indexed;
    populate(reference);
    populate(indexed);
    EXPECT_FALSE(internal::ObjectStoreMutator::eraseExisting(reference, ObjectId::fromUint64(999U)));
    EXPECT_FALSE(internal::ObjectStoreMutator::eraseExisting(indexed, ObjectId::fromUint64(999U)));
    EXPECT_EQ(snapshot(reference), snapshot(indexed));
    EXPECT_TRUE(internal::ObjectStoreMutator::indexMatchesRebuild(indexed));
}

TEST(G108ReferenceDifferential, AcceptedReplayCorpusHasProjectionParityAtEveryCheckpoint) {
    const auto trace = canvas::verification::g1_07::makeAllOperationFamiliesTrace();
    const canvas::verification::g1_07::ReplayInspector reference(
        trace, canvas::verification::g1_07::Provider::kReference);
    const canvas::verification::g1_07::ReplayInspector indexed(
        trace, canvas::verification::g1_07::Provider::kIndexed);
    for (const std::size_t position : {0U, 5U, 10U, 15U}) {
        const auto left = reference.seek(position);
        const auto right = indexed.seek(position);
        ASSERT_TRUE(left.success);
        ASSERT_TRUE(right.success);
        EXPECT_EQ(left.projection_json, right.projection_json);
        EXPECT_EQ(left.semantic_generation, right.semantic_generation);
        EXPECT_EQ(left.commit_ordinal, right.commit_ordinal);
    }
    const auto left_run = reference.run();
    const auto right_run = indexed.run();
    ASSERT_TRUE(left_run.success);
    ASSERT_TRUE(right_run.success);
    EXPECT_EQ(left_run.projection_json, right_run.projection_json);
    EXPECT_EQ(left_run.semantic_generation, right_run.semantic_generation);
    EXPECT_EQ(left_run.commit_ordinal, right_run.commit_ordinal);
}

} // namespace canvas::semantic
