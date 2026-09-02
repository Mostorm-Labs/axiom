#include "canvas/semantic/applied_operation_ledger.hpp"
#include "canvas/semantic/canonical_commit_clock.hpp"
#include "canvas/semantic/canonical_commit_record.hpp"
#include "canvas/semantic/commit_publication_projection.hpp"
#include "canvas/semantic/indexed_object_store.hpp"
#include "canvas/semantic/operation_engine.hpp"
#include "canvas/semantic/reference_object_store.hpp"

#include "object_store_mutator.hpp"

#include <gtest/gtest.h>

#include <array>
#include <cstddef>
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

Operation insertOperation(std::uint64_t operation_id, std::uint64_t object_id) {
    Operation operation{};
    operation.id = OperationId{id(operation_id)};
    operation.document_id = DocumentId{id(1000U)};
    operation.schema_version = 1U;
    operation.payload_version = 1U;
    operation.payload = InsertObjectsOp{{shape(object_id, 7U)}};
    return operation;
}

Operation deleteOperation(std::uint64_t operation_id, std::uint64_t object_id) {
    Operation operation{};
    operation.id = OperationId{id(operation_id)};
    operation.document_id = DocumentId{id(1000U)};
    operation.schema_version = 1U;
    operation.payload_version = 1U;
    operation.payload = DeleteObjectsOp{{id(object_id)}};
    return operation;
}

struct ExpectedProjection final {
    ApplySource source;
    LocalBridgePublicationDisposition disposition;
};

constexpr std::array<ExpectedProjection, 6> kExpectedProjections{{
    {ApplySource::kLocalInteraction, LocalBridgePublicationDisposition::kEligible},
    {ApplySource::kLocalCommand, LocalBridgePublicationDisposition::kEligible},
    {ApplySource::kUndoRedo, LocalBridgePublicationDisposition::kEligible},
    {ApplySource::kLocalAIImport, LocalBridgePublicationDisposition::kEligible},
    {ApplySource::kRestoreReplay, LocalBridgePublicationDisposition::kNoEcho},
    {ApplySource::kRemoteSync, LocalBridgePublicationDisposition::kNoEcho},
}};

template <typename Store>
ApplyResult applyInsert(
    Store& objects,
    AppliedOperationLedger& ledger,
    SemanticGenerationState& generation,
    CanonicalCommitClock& clock,
    ApplySource source,
    std::uint64_t operation_id = 501U,
    std::uint64_t object_id = 1U) {
    return OperationEngine{}.apply(
        insertOperation(operation_id, object_id),
        source,
        objects,
        ledger,
        generation,
        clock);
}

void expectCreatedChange(const CanonicalCommitRecord& record, ObjectId expected_id) {
    EXPECT_EQ(record.change_set.beforeGeneration(), SemanticGeneration(10U));
    EXPECT_EQ(record.change_set.afterGeneration(), SemanticGeneration(11U));
    ASSERT_EQ(record.change_set.objects().size(), 1U);
    EXPECT_EQ(record.change_set.objects().front().object_id, expected_id);
    EXPECT_EQ(record.change_set.objects().front().flags, SemanticChangeFlags::kCreated);
    EXPECT_TRUE(record.change_set.objects().front().changed_fields.empty());
}

void expectSameSemanticRecordFields(
    const CanonicalCommitRecord& left,
    const CanonicalCommitRecord& right) {
    EXPECT_EQ(left.operation_id, right.operation_id);
    EXPECT_EQ(left.before_generation, right.before_generation);
    EXPECT_EQ(left.after_generation, right.after_generation);
    EXPECT_EQ(left.commit_stamp.runtime_epoch, right.commit_stamp.runtime_epoch);
    EXPECT_EQ(left.commit_stamp.ordinal, right.commit_stamp.ordinal);
    EXPECT_EQ(left.change_set.beforeGeneration(), right.change_set.beforeGeneration());
    EXPECT_EQ(left.change_set.afterGeneration(), right.change_set.afterGeneration());
    ASSERT_EQ(left.change_set.objects().size(), right.change_set.objects().size());
    for (std::size_t index = 0; index < left.change_set.objects().size(); ++index) {
        const ObjectSemanticChange& left_change = left.change_set.objects()[index];
        const ObjectSemanticChange& right_change = right.change_set.objects()[index];
        EXPECT_EQ(left_change.object_id, right_change.object_id);
        EXPECT_EQ(left_change.flags, right_change.flags);
        EXPECT_EQ(left_change.changed_fields, right_change.changed_fields);
    }
}

template <typename Store>
void expectNoPublicationFactForAlreadyAppliedAndRejected() {
    Store objects;
    AppliedOperationLedger ledger;
    SemanticGenerationState generation;
    CanonicalCommitClock clock(RuntimeEpoch(42U));
    const Operation first = insertOperation(502U, 2U);
    ASSERT_EQ(
        OperationEngine{}.apply(
            first,
            ApplySource::kLocalCommand,
            objects,
            ledger,
            generation,
            clock)
            .disposition,
        ApplyDisposition::kApplied);

    const auto before_objects = objects.allObjects();
    const SemanticGeneration before_generation = generation.current();
    const CommitOrdinal before_ordinal = clock.lastCommittedOrdinal();

    const ApplyResult already = OperationEngine{}.apply(
        first,
        ApplySource::kRemoteSync,
        objects,
        ledger,
        generation,
        clock);
    EXPECT_EQ(already.disposition, ApplyDisposition::kAlreadyApplied);
    EXPECT_FALSE(already.commit_record.has_value());

    const ApplyResult rejected = OperationEngine{}.apply(
        deleteOperation(502U, 2U),
        ApplySource::kUndoRedo,
        objects,
        ledger,
        generation,
        clock);
    EXPECT_EQ(rejected.disposition, ApplyDisposition::kRejected);
    EXPECT_EQ(rejected.error.issue, StatefulIssue::kOperationIdCollision);
    EXPECT_FALSE(rejected.commit_record.has_value());

    EXPECT_EQ(objects.allObjects(), before_objects);
    EXPECT_EQ(generation.current(), before_generation);
    EXPECT_EQ(clock.lastCommittedOrdinal(), before_ordinal);
    if constexpr (std::is_same_v<Store, IndexedObjectStore>) {
        EXPECT_TRUE(internal::ObjectStoreMutator::indexMatchesRebuild(objects));
    }
}

template <typename Store>
void expectCommitBlockedWithoutPublicationFact() {
    Store objects;
    AppliedOperationLedger ledger;
    SemanticGenerationState generation(SemanticGeneration(20U));
    CanonicalCommitClock invalid_clock;
    const ApplyResult invalid_epoch = applyInsert(
        objects,
        ledger,
        generation,
        invalid_clock,
        ApplySource::kLocalInteraction,
        601U,
        6U);
    EXPECT_EQ(invalid_epoch.disposition, ApplyDisposition::kCommitBlocked);
    EXPECT_EQ(invalid_epoch.commit_block_reason, CommitBlockReason::kInvalidRuntimeEpoch);
    EXPECT_FALSE(invalid_epoch.commit_record.has_value());
    EXPECT_TRUE(objects.allObjects().empty());
    EXPECT_EQ(generation.current(), SemanticGeneration(20U));
    EXPECT_EQ(invalid_clock.lastCommittedOrdinal(), CommitOrdinal(0U));

    CanonicalCommitClock exhausted_clock(
        RuntimeEpoch(44U),
        CommitOrdinal(std::numeric_limits<std::uint64_t>::max() - 1U));
    const ApplyResult first = applyInsert(
        objects,
        ledger,
        generation,
        exhausted_clock,
        ApplySource::kLocalInteraction,
        602U,
        7U);
    ASSERT_EQ(first.disposition, ApplyDisposition::kApplied);
    const auto before_blocked_objects = objects.allObjects();
    const SemanticGeneration before_blocked_generation = generation.current();
    const CommitOrdinal before_blocked_ordinal = exhausted_clock.lastCommittedOrdinal();

    const ApplyResult exhausted = applyInsert(
        objects,
        ledger,
        generation,
        exhausted_clock,
        ApplySource::kRemoteSync,
        603U,
        8U);
    EXPECT_EQ(exhausted.disposition, ApplyDisposition::kCommitBlocked);
    EXPECT_EQ(exhausted.commit_block_reason, CommitBlockReason::kCommitLaneExhausted);
    EXPECT_FALSE(exhausted.commit_record.has_value());
    EXPECT_EQ(objects.allObjects(), before_blocked_objects);
    EXPECT_EQ(generation.current(), before_blocked_generation);
    EXPECT_EQ(exhausted_clock.lastCommittedOrdinal(), before_blocked_ordinal);
    if constexpr (std::is_same_v<Store, IndexedObjectStore>) {
        EXPECT_TRUE(internal::ObjectStoreMutator::indexMatchesRebuild(objects));
    }
}

} // namespace

TEST(OperationEnginePublicationProjection,
     APUB_A4_01_AllSixAppliedSourcesExposeSemanticCommitFactAndExpectedBridgeDisposition) {
    for (const ExpectedProjection& expected : kExpectedProjections) {
        ReferenceObjectStore objects;
        AppliedOperationLedger ledger;
        SemanticGenerationState generation(SemanticGeneration(10U));
        CanonicalCommitClock clock(RuntimeEpoch(41U));

        const ApplyResult result = applyInsert(
            objects,
            ledger,
            generation,
            clock,
            expected.source);
        ASSERT_EQ(result.disposition, ApplyDisposition::kApplied);
        ASSERT_TRUE(result.commit_record.has_value());
        ASSERT_EQ(objects.allObjects().size(), 1U);
        EXPECT_EQ(objects.allObjects().front(), shape(1U, 7U));
        EXPECT_EQ(generation.current(), SemanticGeneration(11U));
        EXPECT_EQ(clock.lastCommittedOrdinal(), CommitOrdinal(1U));
        EXPECT_EQ(result.commit_record->source, expected.source);
        EXPECT_EQ(
            localBridgePublicationDisposition(*result.commit_record),
            expected.disposition);
        expectCreatedChange(*result.commit_record, id(1U));
    }
}

TEST(OperationEnginePublicationProjection,
     APUB_A4_02_LocalRestoreRemotePreserveCanonicalSemanticsAndDifferOnlyInProjection) {
    struct Outcome final {
        ApplyResult result;
        std::vector<ObjectRecord> objects;
    };
    const auto run = [](ApplySource source) {
        ReferenceObjectStore objects;
        AppliedOperationLedger ledger;
        SemanticGenerationState generation(SemanticGeneration(10U));
        CanonicalCommitClock clock(RuntimeEpoch(41U));
        Outcome outcome{
            applyInsert(objects, ledger, generation, clock, source),
            objects.allObjects()};
        return outcome;
    };

    const Outcome local = run(ApplySource::kLocalInteraction);
    const Outcome restore = run(ApplySource::kRestoreReplay);
    const Outcome remote = run(ApplySource::kRemoteSync);
    for (const Outcome* outcome : {&local, &restore, &remote}) {
        ASSERT_EQ(outcome->result.disposition, ApplyDisposition::kApplied);
        ASSERT_TRUE(outcome->result.commit_record.has_value());
        EXPECT_EQ(outcome->objects, std::vector<ObjectRecord>{shape(1U, 7U)});
        expectCreatedChange(*outcome->result.commit_record, id(1U));
    }

    const CanonicalCommitRecord& local_record = *local.result.commit_record;
    const CanonicalCommitRecord& restore_record = *restore.result.commit_record;
    const CanonicalCommitRecord& remote_record = *remote.result.commit_record;
    expectSameSemanticRecordFields(local_record, restore_record);
    expectSameSemanticRecordFields(local_record, remote_record);
    EXPECT_EQ(local_record.source, ApplySource::kLocalInteraction);
    EXPECT_EQ(restore_record.source, ApplySource::kRestoreReplay);
    EXPECT_EQ(remote_record.source, ApplySource::kRemoteSync);
    EXPECT_EQ(
        localBridgePublicationDisposition(local_record),
        LocalBridgePublicationDisposition::kEligible);
    EXPECT_EQ(
        localBridgePublicationDisposition(restore_record),
        LocalBridgePublicationDisposition::kNoEcho);
    EXPECT_EQ(
        localBridgePublicationDisposition(remote_record),
        LocalBridgePublicationDisposition::kNoEcho);

    CanonicalCommitRecord invalid_record = local_record;
    invalid_record.source = static_cast<ApplySource>(0xffU);
    EXPECT_EQ(
        localBridgePublicationDisposition(invalid_record),
        LocalBridgePublicationDisposition::kNoEcho);
}

TEST(OperationEnginePublicationProjection,
     APUB_A4_03_AlreadyAppliedAndRejectedProduceNoPublicationSourceFact) {
    expectNoPublicationFactForAlreadyAppliedAndRejected<ReferenceObjectStore>();
    expectNoPublicationFactForAlreadyAppliedAndRejected<IndexedObjectStore>();
}

TEST(OperationEnginePublicationProjection,
     APUB_A4_04_RuntimeCommitBlocksProduceNoPublicationSourceFact) {
    expectCommitBlockedWithoutPublicationFact<ReferenceObjectStore>();
    expectCommitBlockedWithoutPublicationFact<IndexedObjectStore>();
}

} // namespace canvas::semantic
