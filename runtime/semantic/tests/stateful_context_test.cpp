#include "canvas/semantic/indexed_object_store.hpp"
#include "canvas/semantic/operation_fingerprint.hpp"
#include "canvas/semantic/reference_object_store.hpp"
#include "canvas/semantic/stateful_validation.hpp"

#include "object_store_mutator.hpp"

#include <gtest/gtest.h>

#include <optional>
#include <vector>

namespace canvas::semantic {
namespace {

ObjectRecord record(std::uint64_t id, std::optional<ObjectId> parent = std::nullopt,
                    std::vector<std::uint8_t> order = {1U}) {
    ObjectRecord value{};
    value.id = ObjectId::fromUint64(id);
    value.kind = ObjectKind::kShape;
    value.kind_version = 1U;
    value.placement = Placement{parent, OrderKey(std::move(order))};
    value.content = ShapeContent{7U, 10.0, 20.0};
    return value;
}

class EmptyAppliedOperations final : public AppliedOperationView {
  public:
    std::optional<AppliedOperationEntry> find(const OperationId&) const override { return std::nullopt; }
};

} // namespace

TEST(StatefulValidationContext, KeepsCanonicalStoreReadOnly) {
    ReferenceObjectStore reference;
    IndexedObjectStore indexed;
    ASSERT_TRUE(internal::ObjectStoreMutator::insertFresh(reference, record(1U)));
    ASSERT_TRUE(internal::ObjectStoreMutator::insertFresh(indexed, record(1U)));
    const auto reference_before = reference.allObjects();
    const auto indexed_before = indexed.allObjects();
    EmptyAppliedOperations applied;

    StatefulValidationContext reference_context{reference, applied};
    StatefulValidationContext indexed_context{indexed, applied};
    EXPECT_EQ(reference_context.objects.allObjects(), reference_before);
    EXPECT_EQ(indexed_context.objects.allObjects(), indexed_before);
    EXPECT_EQ(reference.allObjects(), reference_before);
    EXPECT_EQ(indexed.allObjects(), indexed_before);
}

TEST(StatefulValidationContext, StatefulResultDefaultsToOkAndIssuesAreNonOk) {
    const StatefulResult success{};
    const StatefulResult failure{StatefulIssue::kObjectMissing};
    EXPECT_TRUE(success.ok());
    EXPECT_FALSE(failure.ok());

    constexpr StatefulIssue required_issues[] = {
        StatefulIssue::kNone,
        StatefulIssue::kObjectMissing,
        StatefulIssue::kObjectAlreadyExists,
        StatefulIssue::kInvalidKindVersion,
        StatefulIssue::kInvalidApplicability,
        StatefulIssue::kInvalidReference,
        StatefulIssue::kHierarchyCycle,
        StatefulIssue::kConnectorInvalid,
        StatefulIssue::kMaskStateInvalid,
        StatefulIssue::kTextStateInvalid,
        StatefulIssue::kOperationIdCollision,
    };
    EXPECT_EQ(sizeof(required_issues) / sizeof(required_issues[0]), 11U);
}

TEST(StatefulValidationContext, TypedEqualityExcludesOperationIdAndIncludesNestedPayload) {
    Operation left{};
    left.id = OperationId(ObjectId::fromUint64(1U));
    left.document_id = DocumentId(ObjectId::fromUint64(2U));
    left.schema_version = 1U;
    left.payload_version = 1U;
    left.payload = SetVectorPathGeometryOp{
        ObjectId::fromUint64(3U),
        VectorPathGeometry{FillRule::kNonZero, {MoveTo{Vec2{1.0, 2.0}}}}};
    Operation right = left;
    right.id = OperationId(ObjectId::fromUint64(99U));

    EXPECT_TRUE(canonicalPayloadEqual(left, right));

    std::get<SetVectorPathGeometryOp>(right.payload).geometry.commands = {
        MoveTo{Vec2{1.0, 3.0}}};
    EXPECT_FALSE(canonicalPayloadEqual(left, right));
}

TEST(StatefulValidationContext, TypedEqualityIncludesEnvelopeSemanticFields) {
    Operation left{};
    left.document_id = DocumentId(ObjectId::fromUint64(2U));
    left.schema_version = 1U;
    left.payload_version = 1U;
    left.payload = DeleteObjectsOp{{ObjectId::fromUint64(3U)}};
    Operation right = left;

    right.payload_version = 2U;
    EXPECT_FALSE(canonicalPayloadEqual(left, right));
}

TEST(StatefulValidationContext, OptionalFingerprintCannotReplaceTypedEquality) {
    Operation stored{};
    stored.document_id = DocumentId(ObjectId::fromUint64(2U));
    stored.payload = DeleteObjectsOp{{ObjectId::fromUint64(3U)}};
    Operation candidate = stored;
    std::get<DeleteObjectsOp>(candidate.payload).object_ids[0] = ObjectId::fromUint64(4U);

    AppliedOperationEntry entry{stored, OperationFingerprint{0xAAU}};
    ASSERT_TRUE(entry.fingerprint.has_value());
    EXPECT_EQ(*entry.fingerprint, OperationFingerprint({0xAAU}));
    EXPECT_FALSE(canonicalPayloadEqual(entry.canonical_operation, candidate));
}

} // namespace canvas::semantic
