#include "canvas/semantic/applied_operation_ledger.hpp"
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
    std::vector<ObjectRecord> final_objects;
    std::optional<AppliedOperationEntry> applied_entry;
    std::optional<AppliedOperationEntry> rejected_entry;
};

template <typename Store>
SequenceOutcome runSequence(Store& objects, AppliedOperationLedger& ledger) {
    OperationEngine engine;
    const ObjectRecord created = shape(1U, 7U);
    const Operation applied = operation(InsertObjectsOp{{created}}, 201U);
    const Operation collision = operation(DeleteObjectsOp{{created.id}}, 201U);
    const Operation rejected = operation(
        SetTransformsOp{{TransformItem{id(77U), Transform2D{2.0, 0.0, 0.0, 2.0, 1.0, 1.0}}}},
        202U);

    SequenceOutcome outcome;
    for (const Operation& current : std::vector<Operation>{applied, applied, collision, rejected}) {
        const ApplyResult result = engine.apply(current, objects, ledger);
        outcome.dispositions.push_back(result.disposition);
        outcome.issues.push_back(result.error.issue);
    }
    outcome.final_objects = objects.allObjects();
    outcome.applied_entry = ledger.find(applied.id);
    outcome.rejected_entry = ledger.find(rejected.id);
    return outcome;
}

void expectSameCanonicalOperation(const AppliedOperationEntry& actual, const Operation& expected) {
    EXPECT_EQ(actual.canonical_operation.id, expected.id);
    EXPECT_EQ(actual.canonical_operation.document_id, expected.document_id);
    EXPECT_EQ(actual.canonical_operation.schema_version, expected.schema_version);
    EXPECT_EQ(actual.canonical_operation.payload_version, expected.payload_version);
    EXPECT_EQ(actual.canonical_operation.payload, expected.payload);
    EXPECT_FALSE(actual.fingerprint.has_value());
}

} // namespace

TEST(OperationEngineApplyDifferential, ExplicitApplyIdempotencyCollisionAndRejectionSequenceMatchesBothProviders) {
    ReferenceObjectStore reference;
    IndexedObjectStore indexed;
    AppliedOperationLedger reference_ledger;
    AppliedOperationLedger indexed_ledger;
    const ObjectRecord created = shape(1U, 7U);
    const Operation applied = operation(InsertObjectsOp{{created}}, 201U);

    const SequenceOutcome reference_outcome = runSequence(reference, reference_ledger);
    const SequenceOutcome indexed_outcome = runSequence(indexed, indexed_ledger);

    const std::vector<ApplyDisposition> expected_dispositions{
        ApplyDisposition::kApplied,
        ApplyDisposition::kAlreadyApplied,
        ApplyDisposition::kRejected,
        ApplyDisposition::kRejected};
    const std::vector<StatefulIssue> expected_issues{
        StatefulIssue::kNone,
        StatefulIssue::kNone,
        StatefulIssue::kOperationIdCollision,
        StatefulIssue::kObjectMissing};
    const std::vector<ObjectRecord> expected_projection{created};
    EXPECT_EQ(reference_outcome.dispositions, expected_dispositions);
    EXPECT_EQ(indexed_outcome.dispositions, expected_dispositions);
    EXPECT_EQ(reference_outcome.issues, expected_issues);
    EXPECT_EQ(indexed_outcome.issues, expected_issues);
    EXPECT_EQ(reference_outcome.final_objects, expected_projection);
    EXPECT_EQ(indexed_outcome.final_objects, expected_projection);
    ASSERT_TRUE(reference_outcome.applied_entry.has_value());
    ASSERT_TRUE(indexed_outcome.applied_entry.has_value());
    expectSameCanonicalOperation(*reference_outcome.applied_entry, applied);
    expectSameCanonicalOperation(*indexed_outcome.applied_entry, applied);
    EXPECT_FALSE(reference_outcome.rejected_entry.has_value());
    EXPECT_FALSE(indexed_outcome.rejected_entry.has_value());
    EXPECT_TRUE(internal::ObjectStoreMutator::indexMatchesRebuild(indexed));
}

TEST(OperationEngineApplyDifferential, ProductionApplySourceDoesNotScanAllObjects) {
    std::ifstream source(OPERATION_ENGINE_SOURCE_PATH);
    ASSERT_TRUE(source.is_open());
    const std::string contents{std::istreambuf_iterator<char>(source), std::istreambuf_iterator<char>()};
    EXPECT_EQ(contents.find("allObjects("), std::string::npos);
}

} // namespace canvas::semantic
