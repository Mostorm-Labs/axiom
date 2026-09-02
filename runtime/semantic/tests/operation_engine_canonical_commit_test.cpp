#include "canvas/semantic/applied_operation_ledger.hpp"
#include "canvas/semantic/canonical_commit_clock.hpp"
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

Operation operation(OperationPayload payload, std::uint64_t operation_id) {
    Operation result{};
    result.id = OperationId{id(operation_id)};
    result.document_id = DocumentId{id(1000U)};
    result.schema_version = 1U;
    result.payload_version = 1U;
    result.payload = std::move(payload);
    return result;
}

void expectCreatedCommit(
    const ApplyResult& result,
    const Operation& expected_operation,
    ApplySource expected_source,
    SemanticGeneration before_generation,
    SemanticGeneration after_generation,
    RuntimeEpoch expected_epoch,
    CommitOrdinal expected_ordinal,
    ObjectId expected_created_id) {
    ASSERT_EQ(result.disposition, ApplyDisposition::kApplied);
    EXPECT_EQ(result.error.issue, StatefulIssue::kNone);
    EXPECT_EQ(result.commit_block_reason, CommitBlockReason::kNone);
    ASSERT_TRUE(result.commit_record.has_value());
    const CanonicalCommitRecord& record = *result.commit_record;
    EXPECT_EQ(record.operation_id, expected_operation.id);
    EXPECT_EQ(record.source, expected_source);
    EXPECT_EQ(record.before_generation, before_generation);
    EXPECT_EQ(record.after_generation, after_generation);
    EXPECT_EQ(record.commit_stamp.runtime_epoch, expected_epoch);
    EXPECT_EQ(record.commit_stamp.ordinal, expected_ordinal);
    EXPECT_EQ(record.change_set.beforeGeneration(), before_generation);
    EXPECT_EQ(record.change_set.afterGeneration(), after_generation);
    ASSERT_EQ(record.change_set.objects().size(), 1U);
    EXPECT_EQ(record.change_set.objects().front().object_id, expected_created_id);
    EXPECT_EQ(record.change_set.objects().front().flags, SemanticChangeFlags::kCreated);
    EXPECT_TRUE(record.change_set.objects().front().changed_fields.empty());
}

template <typename Store>
void expectFirstCommitHasExactRecord() {
    Store objects;
    AppliedOperationLedger ledger;
    SemanticGenerationState generation(SemanticGeneration(10U));
    CanonicalCommitClock clock(RuntimeEpoch(41U));
    OperationEngine engine;
    const ObjectRecord created = shape(1U, 7U);
    const Operation incoming = operation(InsertObjectsOp{{created}}, 501U);

    const ApplyResult result = engine.apply(
        incoming,
        ApplySource::kLocalInteraction,
        objects,
        ledger,
        generation,
        clock);

    expectCreatedCommit(
        result,
        incoming,
        ApplySource::kLocalInteraction,
        SemanticGeneration(10U),
        SemanticGeneration(11U),
        RuntimeEpoch(41U),
        CommitOrdinal(1U),
        created.id);
    EXPECT_EQ(generation.current(), SemanticGeneration(11U));
    EXPECT_EQ(clock.lastCommittedOrdinal(), CommitOrdinal(1U));
    EXPECT_TRUE(ledger.find(incoming.id).has_value());
    if constexpr (std::is_same_v<Store, IndexedObjectStore>) {
        EXPECT_TRUE(internal::ObjectStoreMutator::indexMatchesRebuild(objects));
    }
}

template <typename Store>
void expectNoCommitFactForAlreadyAppliedOrSemanticRejection() {
    Store objects;
    AppliedOperationLedger ledger;
    SemanticGenerationState generation;
    CanonicalCommitClock clock(RuntimeEpoch(42U));
    OperationEngine engine;
    const Operation first = operation(InsertObjectsOp{{shape(2U, 8U)}}, 502U);
    ASSERT_EQ(
        engine.apply(first, ApplySource::kLocalCommand, objects, ledger, generation, clock).disposition,
        ApplyDisposition::kApplied);

    const auto before_objects = objects.allObjects();
    const SemanticGeneration before_generation = generation.current();
    const CommitOrdinal before_ordinal = clock.lastCommittedOrdinal();
    const ApplyResult already = engine.apply(
        first,
        ApplySource::kRemoteSync,
        objects,
        ledger,
        generation,
        clock);
    EXPECT_EQ(already.disposition, ApplyDisposition::kAlreadyApplied);
    EXPECT_FALSE(already.commit_record.has_value());
    EXPECT_EQ(already.commit_block_reason, CommitBlockReason::kNone);

    const Operation collision = operation(DeleteObjectsOp{{id(2U)}}, 502U);
    const ApplyResult rejected = engine.apply(
        collision,
        ApplySource::kUndoRedo,
        objects,
        ledger,
        generation,
        clock);
    EXPECT_EQ(rejected.disposition, ApplyDisposition::kRejected);
    EXPECT_EQ(rejected.error.issue, StatefulIssue::kOperationIdCollision);
    EXPECT_FALSE(rejected.commit_record.has_value());
    EXPECT_EQ(rejected.commit_block_reason, CommitBlockReason::kNone);

    EXPECT_EQ(objects.allObjects(), before_objects);
    EXPECT_EQ(generation.current(), before_generation);
    EXPECT_EQ(clock.lastCommittedOrdinal(), before_ordinal);
    if constexpr (std::is_same_v<Store, IndexedObjectStore>) {
        EXPECT_TRUE(internal::ObjectStoreMutator::indexMatchesRebuild(objects));
    }
}

} // namespace

TEST(OperationEngineCanonicalCommit, CC_A3_01_FirstAppliedProducesExactRecordForBothProviders) {
    expectFirstCommitHasExactRecord<ReferenceObjectStore>();
    expectFirstCommitHasExactRecord<IndexedObjectStore>();
}

TEST(OperationEngineCanonicalCommit, CC_A3_02_TwoAppliedOperationsAdvanceIndependentOrderedCounters) {
    ReferenceObjectStore objects;
    AppliedOperationLedger ledger;
    SemanticGenerationState generation(SemanticGeneration(50U));
    CanonicalCommitClock clock(RuntimeEpoch(43U), CommitOrdinal(7U));
    OperationEngine engine;
    const ObjectRecord first_object = shape(3U, 9U);
    const ObjectRecord second_object = shape(4U, 10U);

    const ApplyResult first = engine.apply(
        operation(InsertObjectsOp{{first_object}}, 503U),
        ApplySource::kLocalCommand,
        objects,
        ledger,
        generation,
        clock);
    const ApplyResult second = engine.apply(
        operation(InsertObjectsOp{{second_object}}, 504U),
        ApplySource::kLocalAIImport,
        objects,
        ledger,
        generation,
        clock);

    ASSERT_TRUE(first.commit_record.has_value());
    ASSERT_TRUE(second.commit_record.has_value());
    EXPECT_EQ(first.commit_record->commit_stamp.ordinal, CommitOrdinal(8U));
    EXPECT_EQ(second.commit_record->commit_stamp.ordinal, CommitOrdinal(9U));
    EXPECT_LT(first.commit_record->commit_stamp.ordinal, second.commit_record->commit_stamp.ordinal);
    EXPECT_EQ(first.commit_record->after_generation, SemanticGeneration(51U));
    EXPECT_EQ(second.commit_record->after_generation, SemanticGeneration(52U));
    EXPECT_EQ(generation.current(), SemanticGeneration(52U));
    EXPECT_EQ(clock.lastCommittedOrdinal(), CommitOrdinal(9U));
}

TEST(OperationEngineCanonicalCommit, CC_A3_03_And_04_NoCommitFactForAlreadyAppliedOrSemanticRejection) {
    expectNoCommitFactForAlreadyAppliedOrSemanticRejection<ReferenceObjectStore>();
    expectNoCommitFactForAlreadyAppliedOrSemanticRejection<IndexedObjectStore>();
}

TEST(OperationEngineCanonicalCommit, CC_A3_06_And_07_ExhaustionFailsClosedBeforeMutation) {
    ReferenceObjectStore objects;
    AppliedOperationLedger ledger;
    SemanticGenerationState generation(SemanticGeneration(100U));
    CanonicalCommitClock clock(
        RuntimeEpoch(44U), CommitOrdinal(std::numeric_limits<std::uint64_t>::max() - 1U));
    OperationEngine engine;
    const ObjectRecord first_object = shape(5U, 11U);
    const Operation first = operation(InsertObjectsOp{{first_object}}, 505U);

    const ApplyResult at_maximum = engine.apply(
        first,
        ApplySource::kRestoreReplay,
        objects,
        ledger,
        generation,
        clock);
    ASSERT_EQ(at_maximum.disposition, ApplyDisposition::kApplied);
    ASSERT_TRUE(at_maximum.commit_record.has_value());
    EXPECT_EQ(
        at_maximum.commit_record->commit_stamp.ordinal,
        CommitOrdinal(std::numeric_limits<std::uint64_t>::max()));

    const auto before_objects = objects.allObjects();
    const ApplyResult exhausted = engine.apply(
        operation(InsertObjectsOp{{shape(6U, 12U)}}, 506U),
        ApplySource::kRemoteSync,
        objects,
        ledger,
        generation,
        clock);
    EXPECT_EQ(exhausted.disposition, ApplyDisposition::kCommitBlocked);
    EXPECT_EQ(exhausted.commit_block_reason, CommitBlockReason::kCommitLaneExhausted);
    EXPECT_FALSE(exhausted.commit_record.has_value());
    EXPECT_EQ(objects.allObjects(), before_objects);
    EXPECT_EQ(generation.current(), SemanticGeneration(101U));
    EXPECT_EQ(clock.lastCommittedOrdinal(), CommitOrdinal(std::numeric_limits<std::uint64_t>::max()));
    EXPECT_FALSE(ledger.find(OperationId{id(506U)}).has_value());
}

TEST(OperationEngineCanonicalCommit, CC_A3_08_InvalidEpochFailsClosedBeforeMutation) {
    ReferenceObjectStore objects;
    AppliedOperationLedger ledger;
    SemanticGenerationState generation(SemanticGeneration(20U));
    CanonicalCommitClock clock;
    OperationEngine engine;
    const Operation incoming = operation(InsertObjectsOp{{shape(7U, 13U)}}, 507U);

    const ApplyResult result = engine.apply(
        incoming,
        ApplySource::kLocalInteraction,
        objects,
        ledger,
        generation,
        clock);

    EXPECT_EQ(result.disposition, ApplyDisposition::kCommitBlocked);
    EXPECT_EQ(result.commit_block_reason, CommitBlockReason::kInvalidRuntimeEpoch);
    EXPECT_FALSE(result.commit_record.has_value());
    EXPECT_TRUE(objects.allObjects().empty());
    EXPECT_FALSE(ledger.find(incoming.id).has_value());
    EXPECT_EQ(generation.current(), SemanticGeneration(20U));
    EXPECT_EQ(clock.lastCommittedOrdinal(), CommitOrdinal{});
}

TEST(OperationEngineCanonicalCommit, CC_A3_09_RecordRemainsStableWhenCopiedOrMoved) {
    ReferenceObjectStore objects;
    AppliedOperationLedger ledger;
    SemanticGenerationState generation;
    CanonicalCommitClock clock(RuntimeEpoch(45U));
    OperationEngine engine;
    const ApplyResult result = engine.apply(
        operation(InsertObjectsOp{{shape(8U, 14U)}}, 508U),
        ApplySource::kUndoRedo,
        objects,
        ledger,
        generation,
        clock);

    ASSERT_TRUE(result.commit_record.has_value());
    const CanonicalCommitRecord copied = *result.commit_record;
    CanonicalCommitRecord moved = std::move(copied);
    EXPECT_EQ(moved.commit_stamp, result.commit_record->commit_stamp);
    EXPECT_EQ(moved.operation_id, result.commit_record->operation_id);
    EXPECT_EQ(clock.lastCommittedOrdinal(), CommitOrdinal(1U));
}

TEST(OperationEngineCanonicalCommit, CC_A3_10_And_11_EpochAndGenerationNamespacesRemainIndependent) {
    const Operation left_operation = operation(InsertObjectsOp{{shape(9U, 15U)}}, 509U);
    const Operation right_operation = operation(InsertObjectsOp{{shape(9U, 15U)}}, 509U);
    ReferenceObjectStore left_objects;
    ReferenceObjectStore right_objects;
    AppliedOperationLedger left_ledger;
    AppliedOperationLedger right_ledger;
    SemanticGenerationState left_generation(SemanticGeneration(400U));
    SemanticGenerationState right_generation(SemanticGeneration(900U));
    CanonicalCommitClock left_clock(RuntimeEpoch(46U));
    CanonicalCommitClock right_clock(RuntimeEpoch(47U));
    OperationEngine engine;

    const ApplyResult left = engine.apply(
        left_operation,
        ApplySource::kLocalCommand,
        left_objects,
        left_ledger,
        left_generation,
        left_clock);
    const ApplyResult right = engine.apply(
        right_operation,
        ApplySource::kRestoreReplay,
        right_objects,
        right_ledger,
        right_generation,
        right_clock);

    ASSERT_TRUE(left.commit_record.has_value());
    ASSERT_TRUE(right.commit_record.has_value());
    EXPECT_EQ(left.commit_record->commit_stamp.ordinal, CommitOrdinal(1U));
    EXPECT_EQ(right.commit_record->commit_stamp.ordinal, CommitOrdinal(1U));
    EXPECT_NE(left.commit_record->commit_stamp.runtime_epoch, right.commit_record->commit_stamp.runtime_epoch);
    EXPECT_EQ(left.commit_record->after_generation, SemanticGeneration(401U));
    EXPECT_EQ(right.commit_record->after_generation, SemanticGeneration(901U));
}

TEST(OperationEngineCanonicalCommit, SOURCE_A3_01_And_02_SourceIsExplicitRecordContextOnly) {
    const Operation local_operation = operation(InsertObjectsOp{{shape(10U, 16U)}}, 510U);
    const Operation remote_operation = operation(InsertObjectsOp{{shape(10U, 16U)}}, 510U);
    ReferenceObjectStore local_objects;
    ReferenceObjectStore remote_objects;
    AppliedOperationLedger local_ledger;
    AppliedOperationLedger remote_ledger;
    SemanticGenerationState local_generation;
    SemanticGenerationState remote_generation;
    CanonicalCommitClock local_clock(RuntimeEpoch(48U));
    CanonicalCommitClock remote_clock(RuntimeEpoch(48U));
    OperationEngine engine;

    const ApplyResult local = engine.apply(
        local_operation,
        ApplySource::kLocalInteraction,
        local_objects,
        local_ledger,
        local_generation,
        local_clock);
    const ApplyResult remote = engine.apply(
        remote_operation,
        ApplySource::kRemoteSync,
        remote_objects,
        remote_ledger,
        remote_generation,
        remote_clock);

    ASSERT_TRUE(local.commit_record.has_value());
    ASSERT_TRUE(remote.commit_record.has_value());
    EXPECT_EQ(local.commit_record->source, ApplySource::kLocalInteraction);
    EXPECT_EQ(remote.commit_record->source, ApplySource::kRemoteSync);
    EXPECT_EQ(local.commit_record->commit_stamp, remote.commit_record->commit_stamp);
    EXPECT_EQ(local_objects.allObjects(), remote_objects.allObjects());
    EXPECT_EQ(local.commit_record->change_set.objects().size(), remote.commit_record->change_set.objects().size());
    EXPECT_EQ(
        local.commit_record->change_set.objects().front().flags,
        remote.commit_record->change_set.objects().front().flags);
}

} // namespace canvas::semantic
