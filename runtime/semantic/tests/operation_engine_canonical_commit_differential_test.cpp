#include "canvas/semantic/applied_operation_ledger.hpp"
#include "canvas/semantic/canonical_commit_clock.hpp"
#include "canvas/semantic/indexed_object_store.hpp"
#include "canvas/semantic/operation_engine.hpp"
#include "canvas/semantic/reference_object_store.hpp"

#include "object_store_mutator.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <fstream>
#include <iterator>
#include <optional>
#include <string>
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

struct SequenceOutcome final {
    std::vector<ApplyDisposition> dispositions;
    std::vector<StatefulIssue> issues;
    std::vector<CommitBlockReason> block_reasons;
    std::vector<SemanticGeneration> generations;
    std::vector<CommitOrdinal> ordinals;
    std::vector<std::optional<CanonicalCommitRecord>> records;
    std::vector<ObjectRecord> final_objects;
};

template <typename Store>
SequenceOutcome runSequence(Store& objects, AppliedOperationLedger& ledger) {
    OperationEngine engine;
    SemanticGenerationState generation(SemanticGeneration(5U));
    CanonicalCommitClock clock(RuntimeEpoch(70U), CommitOrdinal(20U));
    const ObjectRecord created = shape(6U, 12U);
    const Operation applied = operation(InsertObjectsOp{{created}}, 601U);
    const Operation transformed = operation(
        SetTransformsOp{{TransformItem{created.id, Transform2D{2.0, 0.0, 0.0, 2.0, 20.0, 0.0}}}},
        602U);
    const Operation already = applied;
    const Operation collision = operation(DeleteObjectsOp{{created.id}}, 601U);
    const Operation rejected = operation(
        SetTransformsOp{{TransformItem{id(77U), Transform2D{2.0, 0.0, 0.0, 2.0, 1.0, 1.0}}}},
        603U);
    const std::vector<ApplySource> sources{
        ApplySource::kLocalCommand,
        ApplySource::kRemoteSync,
        ApplySource::kUndoRedo,
        ApplySource::kLocalAIImport,
        ApplySource::kRestoreReplay};

    SequenceOutcome outcome;
    const std::vector<Operation> operations{applied, transformed, already, collision, rejected};
    for (std::size_t index = 0; index < operations.size(); ++index) {
        const ApplyResult result = engine.apply(
            operations[index], sources[index], objects, ledger, generation, clock);
        outcome.dispositions.push_back(result.disposition);
        outcome.issues.push_back(result.error.issue);
        outcome.block_reasons.push_back(result.commit_block_reason);
        outcome.generations.push_back(generation.current());
        outcome.ordinals.push_back(clock.lastCommittedOrdinal());
        outcome.records.push_back(result.commit_record);
    }
    outcome.final_objects = objects.allObjects();
    return outcome;
}

void expectSameRecord(
    const std::optional<CanonicalCommitRecord>& actual,
    const std::optional<CanonicalCommitRecord>& expected) {
    ASSERT_EQ(actual.has_value(), expected.has_value());
    if (!actual.has_value()) {
        return;
    }
    EXPECT_EQ(actual->operation_id, expected->operation_id);
    EXPECT_EQ(actual->source, expected->source);
    EXPECT_EQ(actual->before_generation, expected->before_generation);
    EXPECT_EQ(actual->after_generation, expected->after_generation);
    EXPECT_EQ(actual->commit_stamp, expected->commit_stamp);
    EXPECT_EQ(actual->change_set.beforeGeneration(), expected->change_set.beforeGeneration());
    EXPECT_EQ(actual->change_set.afterGeneration(), expected->change_set.afterGeneration());
    ASSERT_EQ(actual->change_set.objects().size(), expected->change_set.objects().size());
    for (std::size_t index = 0; index < actual->change_set.objects().size(); ++index) {
        EXPECT_EQ(actual->change_set.objects()[index].object_id, expected->change_set.objects()[index].object_id);
        EXPECT_EQ(actual->change_set.objects()[index].flags, expected->change_set.objects()[index].flags);
        EXPECT_EQ(actual->change_set.objects()[index].changed_fields, expected->change_set.objects()[index].changed_fields);
    }
}

} // namespace

TEST(OperationEngineCanonicalCommitDifferential, ReferenceAndIndexedShareExplicitCommitSequence) {
    ReferenceObjectStore reference;
    IndexedObjectStore indexed;
    AppliedOperationLedger reference_ledger;
    AppliedOperationLedger indexed_ledger;
    const SequenceOutcome reference_outcome = runSequence(reference, reference_ledger);
    const SequenceOutcome indexed_outcome = runSequence(indexed, indexed_ledger);

    const std::vector<ApplyDisposition> expected_dispositions{
        ApplyDisposition::kApplied,
        ApplyDisposition::kApplied,
        ApplyDisposition::kAlreadyApplied,
        ApplyDisposition::kRejected,
        ApplyDisposition::kRejected};
    const std::vector<StatefulIssue> expected_issues{
        StatefulIssue::kNone,
        StatefulIssue::kNone,
        StatefulIssue::kNone,
        StatefulIssue::kOperationIdCollision,
        StatefulIssue::kObjectMissing};
    const std::vector<CommitBlockReason> expected_block_reasons(5U, CommitBlockReason::kNone);
    const std::vector<SemanticGeneration> expected_generations{
        SemanticGeneration(6U),
        SemanticGeneration(7U),
        SemanticGeneration(7U),
        SemanticGeneration(7U),
        SemanticGeneration(7U)};
    const std::vector<CommitOrdinal> expected_ordinals{
        CommitOrdinal(21U),
        CommitOrdinal(22U),
        CommitOrdinal(22U),
        CommitOrdinal(22U),
        CommitOrdinal(22U)};
    ObjectRecord expected_transformed = shape(6U, 12U);
    expected_transformed.transform = Transform2D{2.0, 0.0, 0.0, 2.0, 20.0, 0.0};

    EXPECT_EQ(reference_outcome.dispositions, expected_dispositions);
    EXPECT_EQ(indexed_outcome.dispositions, expected_dispositions);
    EXPECT_EQ(reference_outcome.issues, expected_issues);
    EXPECT_EQ(indexed_outcome.issues, expected_issues);
    EXPECT_EQ(reference_outcome.block_reasons, expected_block_reasons);
    EXPECT_EQ(indexed_outcome.block_reasons, expected_block_reasons);
    EXPECT_EQ(reference_outcome.generations, expected_generations);
    EXPECT_EQ(indexed_outcome.generations, expected_generations);
    EXPECT_EQ(reference_outcome.ordinals, expected_ordinals);
    EXPECT_EQ(indexed_outcome.ordinals, expected_ordinals);
    EXPECT_EQ(reference_outcome.final_objects, std::vector<ObjectRecord>{expected_transformed});
    EXPECT_EQ(indexed_outcome.final_objects, std::vector<ObjectRecord>{expected_transformed});
    ASSERT_EQ(reference_outcome.records.size(), indexed_outcome.records.size());
    for (std::size_t index = 0; index < reference_outcome.records.size(); ++index) {
        expectSameRecord(reference_outcome.records[index], indexed_outcome.records[index]);
    }
    ASSERT_TRUE(reference_outcome.records[0].has_value());
    ASSERT_TRUE(reference_outcome.records[1].has_value());
    EXPECT_EQ(reference_outcome.records[0]->source, ApplySource::kLocalCommand);
    EXPECT_EQ(reference_outcome.records[1]->source, ApplySource::kRemoteSync);
    EXPECT_FALSE(reference_outcome.records[2].has_value());
    EXPECT_FALSE(reference_outcome.records[3].has_value());
    EXPECT_FALSE(reference_outcome.records[4].has_value());
    EXPECT_TRUE(internal::ObjectStoreMutator::indexMatchesRebuild(indexed));
}

TEST(OperationEngineCanonicalCommitDifferential, ProductionCommitPathDoesNotScanWholeDocument) {
    std::ifstream source(OPERATION_ENGINE_SOURCE_PATH);
    ASSERT_TRUE(source.is_open());
    const std::string contents{std::istreambuf_iterator<char>(source), std::istreambuf_iterator<char>()};
    EXPECT_EQ(contents.find("allObjects("), std::string::npos);
    EXPECT_EQ(contents.find("publication"), std::string::npos);
    EXPECT_EQ(contents.find("DataBridge"), std::string::npos);
}

} // namespace canvas::semantic
