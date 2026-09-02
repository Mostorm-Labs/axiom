#include "canvas/semantic/idempotency.hpp"
#include "canvas/semantic/stateful_validation.hpp"

#include <gtest/gtest.h>

#include <map>
#include <type_traits>

namespace canvas::semantic {
namespace {

class AppliedOperationsFixture final : public AppliedOperationView {
  public:
    [[nodiscard]] std::optional<AppliedOperationEntry> find(
        const OperationId& id) const override {
        ++lookup_count;
        const auto it = entries.find(id);
        return it == entries.end() ? std::nullopt : std::optional<AppliedOperationEntry>(it->second);
    }

    std::map<OperationId, AppliedOperationEntry> entries;
    mutable std::size_t lookup_count = 0U;
};

Operation makeDeleteOperation(std::uint64_t operation_id, std::uint64_t object_id = 7U) {
    Operation operation{};
    operation.id = OperationId(ObjectId::fromUint64(operation_id));
    operation.document_id = DocumentId(ObjectId::fromUint64(100U));
    operation.schema_version = 1U;
    operation.payload_version = 1U;
    operation.payload = DeleteObjectsOp{{ObjectId::fromUint64(object_id)}};
    return operation;
}

Operation makeRestoreOperation(std::uint64_t operation_id, std::uint64_t object_id = 8U) {
    Operation operation{};
    operation.id = OperationId(ObjectId::fromUint64(operation_id));
    operation.document_id = DocumentId(ObjectId::fromUint64(100U));
    operation.schema_version = 1U;
    operation.payload_version = 1U;
    ObjectRecord record{};
    record.id = ObjectId::fromUint64(object_id);
    record.kind = ObjectKind::kShape;
    record.kind_version = 1U;
    record.placement.order_key = OrderKey({1U});
    record.content = ShapeContent{1U, 10.0, 20.0};
    operation.payload = RestoreObjectsOp{{record}};
    return operation;
}

void store(AppliedOperationsFixture& applied, const Operation& operation) {
    applied.entries.emplace(operation.id, AppliedOperationEntry{operation, std::nullopt});
}

} // namespace

TEST(Idempotency, NewIdIsNotFound) {
    AppliedOperationsFixture applied;
    const Operation incoming = makeDeleteOperation(1U);

    const IdempotencyResult result = classifyOperation(incoming, applied);

    EXPECT_EQ(result.disposition, IdempotencyDisposition::kNew);
    EXPECT_EQ(applied.lookup_count, 1U);
}

TEST(Idempotency, EquivalentPayloadIsAlreadyApplied) {
    AppliedOperationsFixture applied;
    const Operation stored_operation = makeDeleteOperation(2U);
    store(applied, stored_operation);
    const Operation incoming = stored_operation;

    EXPECT_EQ(classifyOperation(incoming, applied).disposition,
              IdempotencyDisposition::kAlreadyApplied);
    EXPECT_EQ(applied.lookup_count, 1U);
}

TEST(Idempotency, DifferentPayloadIsCollision) {
    AppliedOperationsFixture applied;
    const Operation stored_operation = makeDeleteOperation(3U, 7U);
    store(applied, stored_operation);
    const Operation incoming = makeDeleteOperation(3U, 9U);

    EXPECT_EQ(classifyOperation(incoming, applied).disposition,
              IdempotencyDisposition::kCollision);
    EXPECT_EQ(applied.lookup_count, 1U);
}

TEST(Idempotency, OperationIdLookupControlsIdentity) {
    AppliedOperationsFixture applied;
    const Operation stored_operation = makeDeleteOperation(4U);
    store(applied, stored_operation);
    const Operation incoming = makeDeleteOperation(5U);

    EXPECT_EQ(classifyOperation(incoming, applied).disposition,
              IdempotencyDisposition::kNew);
    EXPECT_EQ(applied.lookup_count, 1U);
}

TEST(Idempotency, EnvelopeSemanticDifferenceCollides) {
    AppliedOperationsFixture applied;
    const Operation stored_operation = makeDeleteOperation(6U);
    store(applied, stored_operation);
    Operation incoming = stored_operation;
    incoming.payload_version = 2U;

    EXPECT_EQ(classifyOperation(incoming, applied).disposition,
              IdempotencyDisposition::kCollision);
}

TEST(Idempotency, EquivalentRestoreReplayStopsAtIdempotency) {
    AppliedOperationsFixture applied;
    const Operation stored_operation = makeRestoreOperation(7U);
    store(applied, stored_operation);
    const Operation incoming = stored_operation;

    EXPECT_EQ(classifyOperation(incoming, applied).disposition,
              IdempotencyDisposition::kAlreadyApplied);
    EXPECT_EQ(applied.lookup_count, 1U);
}

TEST(Idempotency, NewOperationIdSameRestorePayloadIsNew) {
    AppliedOperationsFixture applied;
    const Operation stored_operation = makeRestoreOperation(8U);
    store(applied, stored_operation);
    const Operation incoming = makeRestoreOperation(9U);

    EXPECT_EQ(classifyOperation(incoming, applied).disposition,
              IdempotencyDisposition::kNew);
}

TEST(Idempotency, NoAppliedViewMutation) {
    AppliedOperationsFixture applied;
    const Operation stored_operation = makeDeleteOperation(10U);
    store(applied, stored_operation);
    const auto before = applied.entries;
    const Operation incoming = makeDeleteOperation(10U);

    static_cast<void>(classifyOperation(incoming, applied));

    ASSERT_EQ(applied.entries.size(), before.size());
    for (const auto& [id, entry] : before) {
        const auto current = applied.entries.find(id);
        ASSERT_NE(current, applied.entries.end());
        EXPECT_TRUE(canonicalPayloadEqual(current->second.canonical_operation, entry.canonical_operation));
        EXPECT_EQ(current->second.fingerprint, entry.fingerprint);
    }
    EXPECT_EQ(applied.lookup_count, 1U);
}

TEST(Idempotency, ClassifierHasNoObjectStoreDependency) {
    static_assert(std::is_same_v<decltype(&classifyOperation),
                                 IdempotencyResult (*)(const Operation&, const AppliedOperationView&)>);
}

} // namespace canvas::semantic
