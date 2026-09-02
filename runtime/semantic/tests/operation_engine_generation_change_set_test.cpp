#include "canvas/semantic/applied_operation_ledger.hpp"
#include "canvas/semantic/indexed_object_store.hpp"
#include "canvas/semantic/operation_engine.hpp"
#include "canvas/semantic/reference_object_store.hpp"

#include "object_store_mutator.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <limits>
#include <optional>
#include <type_traits>
#include <utility>
#include <vector>

namespace canvas::semantic {
namespace {

ObjectId id(std::uint64_t value) { return ObjectId::fromUint64(value); }

ObjectRecord shape(std::uint64_t value, std::uint8_t tag) {
    ObjectRecord record{};
    record.id = id(value);
    record.kind = ObjectKind::kShape;
    record.kind_version = 1U;
    record.placement = Placement{std::nullopt, OrderKey({tag})};
    record.transform = Transform2D{1.0, 0.0, 0.0, 1.0, static_cast<double>(tag), 0.0};
    record.content = ShapeContent{tag, static_cast<double>(tag), static_cast<double>(tag + 1U)};
    return record;
}

ObjectRecord group(std::uint64_t value, std::optional<ObjectId> parent = std::nullopt) {
    ObjectRecord record{};
    record.id = id(value);
    record.kind = ObjectKind::kGroup;
    record.kind_version = 1U;
    record.placement = Placement{parent, OrderKey({static_cast<std::uint8_t>(value)})};
    record.content = GroupContent{};
    return record;
}

ObjectRecord nestedShape(std::uint64_t value, ObjectId parent) {
    ObjectRecord record = shape(value, static_cast<std::uint8_t>(value));
    record.placement.parent_id = parent;
    return record;
}

ObjectRecord attachedConnector(
    std::uint64_t value,
    ObjectId start,
    std::optional<ObjectId> end = std::nullopt) {
    ObjectRecord record{};
    record.id = id(value);
    record.kind = ObjectKind::kConnector;
    record.kind_version = 1U;
    record.placement = Placement{std::nullopt, OrderKey({static_cast<std::uint8_t>(value)})};
    ConnectorContent content{};
    content.start.value = AttachedEndpoint{start, AutoPerimeterAnchor{}};
    if (end.has_value()) {
        content.end.value = AttachedEndpoint{*end, AutoPerimeterAnchor{}};
    }
    record.content = content;
    return record;
}

Operation operation(OperationPayload payload, std::uint64_t operation_id) {
    Operation result{};
    result.id = OperationId{id(operation_id)};
    result.document_id = DocumentId{id(1000U)};
    result.schema_version = 1U;
    result.payload_version = 1U;
    result.payload = std::move(payload);
    return result;
}

void expectSingleChange(
    const ApplyResult& result,
    SemanticGeneration before,
    SemanticGeneration after,
    ObjectId object_id,
    SemanticChangeFlags flags,
    std::vector<FieldId> fields = {}) {
    ASSERT_EQ(result.disposition, ApplyDisposition::kApplied);
    ASSERT_TRUE(result.commit_record.has_value());
    const ChangeSet& change_set = result.commit_record->change_set;
    EXPECT_EQ(change_set.beforeGeneration(), before);
    EXPECT_EQ(change_set.afterGeneration(), after);
    ASSERT_EQ(change_set.objects().size(), 1U);
    const auto& change = change_set.objects().front();
    EXPECT_EQ(change.object_id, object_id);
    EXPECT_EQ(change.flags, flags);
    EXPECT_EQ(change.changed_fields, fields);
}

template <typename Store>
void expectFirstInsertAdvancesGenerationAndBuildsChangeSet() {
    Store objects;
    AppliedOperationLedger ledger;
    SemanticGenerationState generation(SemanticGeneration(7U));
    CanonicalCommitClock canonical_commit_clock(RuntimeEpoch(3U));
    OperationEngine engine;
    const ObjectRecord created = shape(1U, 7U);
    const ApplyResult result = engine.apply(
        operation(InsertObjectsOp{{created}}, 301U),
        ApplySource::kLocalInteraction,
        objects,
        ledger,
        generation,
        canonical_commit_clock);

    expectSingleChange(
        result,
        SemanticGeneration(7U),
        SemanticGeneration(8U),
        created.id,
        SemanticChangeFlags::kCreated);
    EXPECT_EQ(generation.current(), SemanticGeneration(8U));
    if constexpr (std::is_same_v<Store, IndexedObjectStore>) {
        EXPECT_TRUE(internal::ObjectStoreMutator::indexMatchesRebuild(objects));
    }
}

template <typename Store>
void expectResolvedDeleteClosureProducesCompleteChangeSet() {
    Store objects;
    AppliedOperationLedger ledger;
    SemanticGenerationState generation(SemanticGeneration(10U));
    CanonicalCommitClock canonical_commit_clock(RuntimeEpoch(3U));
    OperationEngine engine;

    const ObjectRecord root = group(1U);
    const ObjectRecord parent = group(2U, root.id);
    const ObjectRecord child = nestedShape(3U, parent.id);
    const ObjectRecord connector_to_parent = attachedConnector(10U, parent.id);
    const ObjectRecord connector_to_child = attachedConnector(20U, child.id);
    const ObjectRecord sentinel = shape(99U, 99U);
    for (const ObjectRecord& record :
         {root, parent, child, connector_to_parent, connector_to_child, sentinel}) {
        ASSERT_TRUE(internal::ObjectStoreMutator::insertFresh(objects, record));
    }
    const std::vector<ObjectRecord> before = objects.allObjects();
    const Operation deletion = operation(DeleteObjectsOp{{root.id}}, 310U);
    const ApplyResult result = engine.apply(
        deletion,
        ApplySource::kLocalInteraction,
        objects,
        ledger,
        generation,
        canonical_commit_clock);

    ASSERT_EQ(result.disposition, ApplyDisposition::kApplied);
    ASSERT_TRUE(result.commit_record.has_value());
    const ChangeSet& change_set = result.commit_record->change_set;
    EXPECT_EQ(change_set.beforeGeneration(), SemanticGeneration(10U));
    EXPECT_EQ(change_set.afterGeneration(), SemanticGeneration(11U));
    EXPECT_EQ(generation.current(), SemanticGeneration(11U));
    ASSERT_EQ(change_set.objects().size(), 5U);
    const std::vector<ObjectId> expected_ids{root.id, parent.id, child.id,
                                             connector_to_parent.id, connector_to_child.id};
    for (std::size_t index = 0; index < expected_ids.size(); ++index) {
        const ObjectSemanticChange& change = change_set.objects()[index];
        EXPECT_EQ(change.object_id, expected_ids[index]);
        EXPECT_EQ(change.flags, SemanticChangeFlags::kDeleted);
        EXPECT_TRUE(change.changed_fields.empty());
        EXPECT_FALSE(objects.contains(change.object_id));
    }
    ASSERT_TRUE(objects.contains(sentinel.id));
    ASSERT_NE(objects.find(sentinel.id), nullptr);
    EXPECT_EQ(*objects.find(sentinel.id), sentinel);
    const auto ledger_entry = ledger.find(deletion.id);
    ASSERT_TRUE(ledger_entry.has_value());
    const auto& ledger_operation = ledger_entry->canonical_operation;
    EXPECT_EQ(ledger_operation.id, deletion.id);
    EXPECT_EQ(ledger_operation.payload.index(), deletion.payload.index());
    EXPECT_EQ(ledger_operation.payload, deletion.payload);
    if constexpr (std::is_same_v<Store, IndexedObjectStore>) {
        EXPECT_TRUE(internal::ObjectStoreMutator::indexMatchesRebuild(objects));
    }
    const std::vector<ObjectRecord> expected_remaining{sentinel};
    EXPECT_EQ(objects.allObjects(), expected_remaining);
    EXPECT_EQ(before.size(), 6U);
}

} // namespace

TEST(OperationEngineGenerationChangeSet, FirstInsertAdvancesAndCreatesForReferenceAndIndexed) {
    expectFirstInsertAdvancesGenerationAndBuildsChangeSet<ReferenceObjectStore>();
    expectFirstInsertAdvancesGenerationAndBuildsChangeSet<IndexedObjectStore>();
}

TEST(OperationEngineGenerationChangeSet, ResolvedDeleteClosureProducesCompleteChangeSetForBothProviders) {
    expectResolvedDeleteClosureProducesCompleteChangeSet<ReferenceObjectStore>();
    expectResolvedDeleteClosureProducesCompleteChangeSet<IndexedObjectStore>();
}

TEST(OperationEngineGenerationChangeSet, AlreadyAppliedAndRejectedAreGenerationNeutralAndChangeSetFree) {
    ReferenceObjectStore objects;
    AppliedOperationLedger ledger;
    SemanticGenerationState generation(SemanticGeneration(4U));
    CanonicalCommitClock canonical_commit_clock(RuntimeEpoch(3U));
    OperationEngine engine;
    const Operation first = operation(InsertObjectsOp{{shape(2U, 8U)}}, 302U);
    ASSERT_EQ(
        engine.apply(
                  first,
                  ApplySource::kLocalInteraction,
                  objects,
                  ledger,
                  generation,
                  canonical_commit_clock)
            .disposition,
        ApplyDisposition::kApplied);

    const ApplyResult already = engine.apply(
        first,
        ApplySource::kLocalInteraction,
        objects,
        ledger,
        generation,
        canonical_commit_clock);
    EXPECT_EQ(already.disposition, ApplyDisposition::kAlreadyApplied);
    EXPECT_FALSE(already.commit_record.has_value());
    EXPECT_EQ(generation.current(), SemanticGeneration(5U));

    const Operation collision = operation(DeleteObjectsOp{{id(2U)}}, 302U);
    const ApplyResult rejected = engine.apply(
        collision,
        ApplySource::kLocalInteraction,
        objects,
        ledger,
        generation,
        canonical_commit_clock);
    EXPECT_EQ(rejected.disposition, ApplyDisposition::kRejected);
    EXPECT_EQ(rejected.error.issue, StatefulIssue::kOperationIdCollision);
    EXPECT_FALSE(rejected.commit_record.has_value());
    EXPECT_EQ(generation.current(), SemanticGeneration(5U));

    const Operation missing = operation(
        SetTransformsOp{{TransformItem{id(99U), Transform2D{2.0, 0.0, 0.0, 2.0, 1.0, 1.0}}}},
        303U);
    const ApplyResult state_rejected = engine.apply(
        missing,
        ApplySource::kLocalInteraction,
        objects,
        ledger,
        generation,
        canonical_commit_clock);
    EXPECT_EQ(state_rejected.disposition, ApplyDisposition::kRejected);
    EXPECT_FALSE(state_rejected.commit_record.has_value());
    EXPECT_EQ(generation.current(), SemanticGeneration(5U));
}

TEST(OperationEngineGenerationChangeSet, SequentialAppliedOperationsChainGenerationIntervalsAndFlags) {
    ReferenceObjectStore objects;
    AppliedOperationLedger ledger;
    SemanticGenerationState generation;
    CanonicalCommitClock canonical_commit_clock(RuntimeEpoch(3U));
    OperationEngine engine;
    const ObjectRecord created = shape(3U, 9U);
    const Operation insert = operation(InsertObjectsOp{{created}}, 304U);
    const ApplyResult insert_result = engine.apply(
        insert,
        ApplySource::kLocalInteraction,
        objects,
        ledger,
        generation,
        canonical_commit_clock);
    expectSingleChange(
        insert_result,
        SemanticGeneration(0U),
        SemanticGeneration(1U),
        created.id,
        SemanticChangeFlags::kCreated);

    ObjectRecord transformed = created;
    transformed.transform.tx = 99.0;
    const ApplyResult transform_result = engine.apply(
        operation(SetTransformsOp{{TransformItem{created.id, transformed.transform}}}, 305U),
        ApplySource::kLocalInteraction,
        objects,
        ledger,
        generation,
        canonical_commit_clock);
    expectSingleChange(
        transform_result,
        SemanticGeneration(1U),
        SemanticGeneration(2U),
        created.id,
        SemanticChangeFlags::kTransform);
    EXPECT_EQ(generation.current(), SemanticGeneration(2U));

    const ApplyResult delete_result = engine.apply(
        operation(DeleteObjectsOp{{created.id}}, 306U),
        ApplySource::kLocalInteraction,
        objects,
        ledger,
        generation,
        canonical_commit_clock);
    expectSingleChange(
        delete_result,
        SemanticGeneration(2U),
        SemanticGeneration(3U),
        created.id,
        SemanticChangeFlags::kDeleted);
}

TEST(OperationEngineGenerationChangeSet, PatchPropertiesCarriesSortedDistinctFieldIds) {
    ReferenceObjectStore objects;
    AppliedOperationLedger ledger;
    SemanticGenerationState generation;
    CanonicalCommitClock canonical_commit_clock(RuntimeEpoch(3U));
    OperationEngine engine;
    const ObjectRecord created = shape(4U, 10U);
    ASSERT_EQ(
        engine.apply(
                  operation(InsertObjectsOp{{created}}, 307U),
                  ApplySource::kLocalInteraction,
                  objects,
                  ledger,
                  generation,
                  canonical_commit_clock)
            .disposition,
        ApplyDisposition::kApplied);

    const PatchPropertiesOp patch{
        {{created.id, 2U, PropertyPatchAction::kSet, PropertyValue{true}},
         {created.id, 1U, PropertyPatchAction::kSet, PropertyValue{false}},
         {created.id, 2U, PropertyPatchAction::kSet, PropertyValue{true}}}};
    const ApplyResult result = engine.apply(
        operation(patch, 308U),
        ApplySource::kLocalInteraction,
        objects,
        ledger,
        generation,
        canonical_commit_clock);
    expectSingleChange(
        result,
        SemanticGeneration(1U),
        SemanticGeneration(2U),
        created.id,
        SemanticChangeFlags::kProperties,
        {1U, 2U});
}

TEST(OperationEngineGenerationChangeSet, GenerationOverflowFailsBeforeMutation) {
    ReferenceObjectStore objects;
    AppliedOperationLedger ledger;
    SemanticGenerationState generation{
        SemanticGeneration{std::numeric_limits<std::uint64_t>::max()}};
    CanonicalCommitClock canonical_commit_clock(RuntimeEpoch(3U));
    OperationEngine engine;
    const ObjectRecord created = shape(5U, 11U);
    const ApplyResult result = engine.apply(
        operation(InsertObjectsOp{{created}}, 309U),
        ApplySource::kLocalInteraction,
        objects,
        ledger,
        generation,
        canonical_commit_clock);

    EXPECT_EQ(result.disposition, ApplyDisposition::kRejected);
    EXPECT_FALSE(result.commit_record.has_value());
    EXPECT_EQ(generation.current(), SemanticGeneration(std::numeric_limits<std::uint64_t>::max()));
    EXPECT_TRUE(objects.allObjects().empty());
    EXPECT_FALSE(ledger.find(OperationId{id(309U)}).has_value());
}

} // namespace canvas::semantic
