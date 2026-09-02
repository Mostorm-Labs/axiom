#include "canvas/semantic/applied_operation_ledger.hpp"
#include "canvas/semantic/indexed_object_store.hpp"
#include "canvas/semantic/operation_engine.hpp"
#include "canvas/semantic/reference_object_store.hpp"

#include "object_store_mutator.hpp"

#include <gtest/gtest.h>

#include <cstdint>
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

void expectCanonicalOperationEqual(const Operation& actual, const Operation& expected) {
    EXPECT_EQ(actual.id, expected.id);
    EXPECT_EQ(actual.document_id, expected.document_id);
    EXPECT_EQ(actual.schema_version, expected.schema_version);
    EXPECT_EQ(actual.payload_version, expected.payload_version);
    EXPECT_EQ(actual.payload, expected.payload);
}

template <typename Store>
void expectFirstInsertAppliesAndRecordsCanonicalOperation() {
    Store objects;
    AppliedOperationLedger ledger;
    SemanticGenerationState generation;
    CanonicalCommitClock canonical_commit_clock(RuntimeEpoch(1U));
    OperationEngine engine;
    const ObjectRecord created = shape(1U, 7U);
    const Operation incoming = operation(InsertObjectsOp{{created}}, 101U);

    const ApplyResult result = engine.apply(
        incoming,
        ApplySource::kLocalInteraction,
        objects,
        ledger,
        generation,
        canonical_commit_clock);

    EXPECT_EQ(result.disposition, ApplyDisposition::kApplied);
    EXPECT_EQ(result.error.issue, StatefulIssue::kNone);
    EXPECT_EQ(objects.allObjects(), std::vector<ObjectRecord>({created}));
    const auto entry = ledger.find(incoming.id);
    ASSERT_TRUE(entry.has_value());
    expectCanonicalOperationEqual(entry->canonical_operation, incoming);
    EXPECT_FALSE(entry->fingerprint.has_value());
    if constexpr (std::is_same_v<Store, IndexedObjectStore>) {
        EXPECT_TRUE(internal::ObjectStoreMutator::indexMatchesRebuild(objects));
    }
}

template <typename Store>
void expectNoCommitDispositionLeavesStateAndLedgerUnchanged(
    const Operation& first,
    const Operation& follow_up,
    ApplyDisposition expected_disposition,
    StatefulIssue expected_issue) {
    Store objects;
    AppliedOperationLedger ledger;
    SemanticGenerationState generation;
    CanonicalCommitClock canonical_commit_clock(RuntimeEpoch(1U));
    OperationEngine engine;
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
    const auto before_objects = objects.allObjects();
    const auto before_entry = ledger.find(first.id);
    ASSERT_TRUE(before_entry.has_value());

    const ApplyResult result = engine.apply(
        follow_up,
        ApplySource::kLocalInteraction,
        objects,
        ledger,
        generation,
        canonical_commit_clock);

    EXPECT_EQ(result.disposition, expected_disposition);
    EXPECT_EQ(result.error.issue, expected_issue);
    EXPECT_EQ(objects.allObjects(), before_objects);
    const auto after_entry = ledger.find(first.id);
    ASSERT_TRUE(after_entry.has_value());
    expectCanonicalOperationEqual(after_entry->canonical_operation, before_entry->canonical_operation);
    EXPECT_EQ(after_entry->fingerprint, before_entry->fingerprint);
    if constexpr (std::is_same_v<Store, IndexedObjectStore>) {
        EXPECT_TRUE(internal::ObjectStoreMutator::indexMatchesRebuild(objects));
    }
}

} // namespace

TEST(OperationEngineApply, FirstSeenInsertAppliesExpectedStateAndRecordsLedgerForBothProviders) {
    expectFirstInsertAppliesAndRecordsCanonicalOperation<ReferenceObjectStore>();
    expectFirstInsertAppliesAndRecordsCanonicalOperation<IndexedObjectStore>();
}

TEST(OperationEngineApply, SameIdAndCanonicalOperationIsAlreadyAppliedWithoutSecondMutation) {
    const Operation first = operation(InsertObjectsOp{{shape(2U, 8U)}}, 102U);
    expectNoCommitDispositionLeavesStateAndLedgerUnchanged<ReferenceObjectStore>(
        first, first, ApplyDisposition::kAlreadyApplied, StatefulIssue::kNone);
    expectNoCommitDispositionLeavesStateAndLedgerUnchanged<IndexedObjectStore>(
        first, first, ApplyDisposition::kAlreadyApplied, StatefulIssue::kNone);
}

TEST(OperationEngineApply, SameIdAndDifferentOperationRejectsCollisionWithoutLedgerOverwrite) {
    const Operation first = operation(InsertObjectsOp{{shape(3U, 9U)}}, 103U);
    const Operation collision = operation(DeleteObjectsOp{{id(3U)}}, 103U);
    expectNoCommitDispositionLeavesStateAndLedgerUnchanged<ReferenceObjectStore>(
        first, collision, ApplyDisposition::kRejected, StatefulIssue::kOperationIdCollision);
    expectNoCommitDispositionLeavesStateAndLedgerUnchanged<IndexedObjectStore>(
        first, collision, ApplyDisposition::kRejected, StatefulIssue::kOperationIdCollision);
}

TEST(OperationEngineApply, FirstSeenStateInvalidOperationRejectsWithoutLedgerEntry) {
    const Operation rejected = operation(
        SetTransformsOp{{TransformItem{id(99U), Transform2D{2.0, 0.0, 0.0, 2.0, 1.0, 1.0}}}},
        104U);

    ReferenceObjectStore reference;
    IndexedObjectStore indexed;
    AppliedOperationLedger reference_ledger;
    AppliedOperationLedger indexed_ledger;
    SemanticGenerationState reference_generation;
    SemanticGenerationState indexed_generation;
    CanonicalCommitClock reference_canonical_commit_clock(RuntimeEpoch(1U));
    CanonicalCommitClock indexed_canonical_commit_clock(RuntimeEpoch(1U));
    OperationEngine engine;
    const auto reference_before = reference.allObjects();
    const auto indexed_before = indexed.allObjects();

    const ApplyResult reference_result = engine.apply(
        rejected,
        ApplySource::kLocalInteraction,
        reference,
        reference_ledger,
        reference_generation,
        reference_canonical_commit_clock);
    const ApplyResult indexed_result = engine.apply(
        rejected,
        ApplySource::kLocalInteraction,
        indexed,
        indexed_ledger,
        indexed_generation,
        indexed_canonical_commit_clock);

    EXPECT_EQ(reference_result.disposition, ApplyDisposition::kRejected);
    EXPECT_EQ(indexed_result.disposition, ApplyDisposition::kRejected);
    EXPECT_EQ(reference_result.error.issue, StatefulIssue::kObjectMissing);
    EXPECT_EQ(indexed_result.error.issue, StatefulIssue::kObjectMissing);
    EXPECT_EQ(reference.allObjects(), reference_before);
    EXPECT_EQ(indexed.allObjects(), indexed_before);
    EXPECT_FALSE(reference_ledger.find(rejected.id).has_value());
    EXPECT_FALSE(indexed_ledger.find(rejected.id).has_value());
    EXPECT_TRUE(internal::ObjectStoreMutator::indexMatchesRebuild(indexed));
}

} // namespace canvas::semantic
