#include "canvas/semantic/applied_operation_ledger.hpp"
#include "canvas/semantic/indexed_object_store.hpp"
#include "canvas/semantic/operation_engine.hpp"
#include "canvas/semantic/reference_object_store.hpp"

#include "object_store_mutator.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <fstream>
#include <iterator>
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
    std::vector<SemanticGeneration> generations;
    std::vector<std::optional<ChangeSet>> change_sets;
    std::vector<ObjectRecord> final_objects;
};

template <typename Store>
SequenceOutcome runSequence(Store& objects, AppliedOperationLedger& ledger) {
    OperationEngine engine;
    SemanticGenerationState generation;
    CanonicalCommitClock canonical_commit_clock(RuntimeEpoch(4U));
    const ObjectRecord created = shape(6U, 12U);
    const Operation applied = operation(InsertObjectsOp{{created}}, 401U);
    const Operation transformed = operation(
        SetTransformsOp{{TransformItem{created.id, Transform2D{2.0, 0.0, 0.0, 2.0, 20.0, 0.0}}}},
        402U);
    const Operation already = applied;
    const Operation collision = operation(DeleteObjectsOp{{created.id}}, 401U);
    const Operation rejected = operation(
        SetTransformsOp{{TransformItem{id(77U), Transform2D{2.0, 0.0, 0.0, 2.0, 1.0, 1.0}}}},
        403U);

    SequenceOutcome outcome;
    for (const Operation& current : {applied, transformed, already, collision, rejected}) {
        const ApplyResult result = engine.apply(
            current,
            ApplySource::kLocalInteraction,
            objects,
            ledger,
            generation,
            canonical_commit_clock);
        outcome.dispositions.push_back(result.disposition);
        outcome.issues.push_back(result.error.issue);
        outcome.generations.push_back(generation.current());
        if (result.commit_record.has_value()) {
            outcome.change_sets.push_back(result.commit_record->change_set);
        } else {
            outcome.change_sets.push_back(std::nullopt);
        }
    }
    outcome.final_objects = objects.allObjects();
    return outcome;
}

void expectChangeSetEquivalent(const std::optional<ChangeSet>& actual, const std::optional<ChangeSet>& expected) {
    ASSERT_EQ(actual.has_value(), expected.has_value());
    if (!actual.has_value()) {
        return;
    }
    EXPECT_EQ(actual->beforeGeneration(), expected->beforeGeneration());
    EXPECT_EQ(actual->afterGeneration(), expected->afterGeneration());
    ASSERT_EQ(actual->objects().size(), expected->objects().size());
    for (std::size_t index = 0; index < actual->objects().size(); ++index) {
        EXPECT_EQ(actual->objects()[index].object_id, expected->objects()[index].object_id);
        EXPECT_EQ(actual->objects()[index].flags, expected->objects()[index].flags);
        EXPECT_EQ(actual->objects()[index].changed_fields, expected->objects()[index].changed_fields);
    }
}

} // namespace

TEST(OperationEngineGenerationChangeSetDifferential, ReferenceAndIndexedShareExplicitExpectedSequence) {
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
    const std::vector<SemanticGeneration> expected_generations{
        SemanticGeneration(1U),
        SemanticGeneration(2U),
        SemanticGeneration(2U),
        SemanticGeneration(2U),
        SemanticGeneration(2U)};
    ObjectRecord expected_transformed = shape(6U, 12U);
    expected_transformed.transform = Transform2D{2.0, 0.0, 0.0, 2.0, 20.0, 0.0};
    const std::vector<ObjectRecord> expected_objects{expected_transformed};
    EXPECT_EQ(reference_outcome.dispositions, expected_dispositions);
    EXPECT_EQ(indexed_outcome.dispositions, expected_dispositions);
    EXPECT_EQ(reference_outcome.issues, expected_issues);
    EXPECT_EQ(indexed_outcome.issues, expected_issues);
    EXPECT_EQ(reference_outcome.generations, expected_generations);
    EXPECT_EQ(indexed_outcome.generations, expected_generations);
    EXPECT_EQ(reference_outcome.final_objects, expected_objects);
    EXPECT_EQ(indexed_outcome.final_objects, expected_objects);
    ASSERT_EQ(reference_outcome.change_sets.size(), indexed_outcome.change_sets.size());
    for (std::size_t index = 0; index < reference_outcome.change_sets.size(); ++index) {
        expectChangeSetEquivalent(reference_outcome.change_sets[index], indexed_outcome.change_sets[index]);
    }
    for (const auto* changes : {&reference_outcome.change_sets, &indexed_outcome.change_sets}) {
        ASSERT_TRUE((*changes)[0].has_value());
        EXPECT_EQ((*changes)[0]->beforeGeneration(), SemanticGeneration(0U));
        EXPECT_EQ((*changes)[0]->afterGeneration(), SemanticGeneration(1U));
        ASSERT_EQ((*changes)[0]->objects().size(), 1U);
        EXPECT_EQ((*changes)[0]->objects()[0].object_id, id(6U));
        EXPECT_EQ((*changes)[0]->objects()[0].flags, SemanticChangeFlags::kCreated);
        EXPECT_TRUE((*changes)[0]->objects()[0].changed_fields.empty());

        ASSERT_TRUE((*changes)[1].has_value());
        EXPECT_EQ((*changes)[1]->beforeGeneration(), SemanticGeneration(1U));
        EXPECT_EQ((*changes)[1]->afterGeneration(), SemanticGeneration(2U));
        ASSERT_EQ((*changes)[1]->objects().size(), 1U);
        EXPECT_EQ((*changes)[1]->objects()[0].object_id, id(6U));
        EXPECT_EQ((*changes)[1]->objects()[0].flags, SemanticChangeFlags::kTransform);
        EXPECT_TRUE((*changes)[1]->objects()[0].changed_fields.empty());

        EXPECT_FALSE((*changes)[2].has_value());
        EXPECT_FALSE((*changes)[3].has_value());
        EXPECT_FALSE((*changes)[4].has_value());
    }
    EXPECT_TRUE(internal::ObjectStoreMutator::indexMatchesRebuild(indexed));
}

TEST(OperationEngineGenerationChangeSetDifferential, ProductionChangeSetPathDoesNotScanWholeDocument) {
    std::ifstream engine_source(OPERATION_ENGINE_SOURCE_PATH);
    ASSERT_TRUE(engine_source.is_open());
    const std::string engine_contents{
        std::istreambuf_iterator<char>(engine_source), std::istreambuf_iterator<char>()};
    EXPECT_EQ(engine_contents.find("allObjects("), std::string::npos);

    std::ifstream builder_source(CHANGE_SET_BUILDER_SOURCE_PATH);
    ASSERT_TRUE(builder_source.is_open());
    const std::string builder_contents{
        std::istreambuf_iterator<char>(builder_source), std::istreambuf_iterator<char>()};
    EXPECT_EQ(builder_contents.find("allObjects("), std::string::npos);
}

} // namespace canvas::semantic
