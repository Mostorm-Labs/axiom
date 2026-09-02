#include "canvas/semantic/indexed_object_store.hpp"
#include "canvas/semantic/operation_engine.hpp"
#include "canvas/semantic/operation_fingerprint.hpp"
#include "canvas/semantic/reference_object_store.hpp"
#include "object_store_mutator.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <map>
#include <optional>
#include <type_traits>
#include <utility>
#include <vector>

namespace canvas::semantic {
namespace {

ObjectId id(std::uint64_t value) { return ObjectId::fromUint64(value); }

ObjectRecord shape(std::uint64_t value) {
    ObjectRecord result{};
    result.id = id(value);
    result.kind = ObjectKind::kShape;
    result.kind_version = 1U;
    result.placement.order_key = OrderKey({static_cast<std::uint8_t>(value)});
    result.content = ShapeContent{1U, 10.0, 20.0};
    return result;
}

template <typename Payload>
Operation operation(std::uint64_t operation_id, Payload payload) {
    Operation result{};
    result.id = OperationId{id(operation_id)};
    result.document_id = DocumentId{id(8002U)};
    result.schema_version = 1U;
    result.payload_version = 1U;
    result.payload = std::move(payload);
    return result;
}

class TestAppliedOperationView final : public AppliedOperationView {
  public:
    std::optional<AppliedOperationEntry> find(const OperationId& operation_id) const override {
        const auto found = entries.find(operation_id);
        return found == entries.end() ? std::nullopt
                                      : std::optional<AppliedOperationEntry>(found->second);
    }

    std::map<OperationId, AppliedOperationEntry> entries;
};

void expectAppliedUnchanged(
    const std::map<OperationId, AppliedOperationEntry>& before,
    const TestAppliedOperationView& after) {
    ASSERT_EQ(after.entries.size(), before.size());
    for (const auto& [operation_id, expected] : before) {
        const auto found = after.entries.find(operation_id);
        ASSERT_NE(found, after.entries.end());
        EXPECT_EQ(found->second.canonical_operation.id, expected.canonical_operation.id);
        EXPECT_TRUE(canonicalPayloadEqual(
            found->second.canonical_operation, expected.canonical_operation));
        EXPECT_EQ(found->second.fingerprint, expected.fingerprint);
    }
}

template <typename Store>
void seed(Store& store, const std::vector<ObjectRecord>& records) {
    for (const auto& record : records) {
        ASSERT_TRUE(internal::ObjectStoreMutator::insertFresh(store, record));
    }
}

template <typename Store>
void expectNoMutation(
    const std::vector<ObjectRecord>& initial_objects,
    const Operation& input,
    TestAppliedOperationView applied,
    PrepareDisposition expected_disposition,
    StatefulIssue expected_issue,
    bool expected_plan) {
    Store store;
    seed(store, initial_objects);

    const auto store_before = store.allObjects();
    const auto applied_before = applied.entries;
    if constexpr (std::is_same_v<Store, IndexedObjectStore>) {
        ASSERT_TRUE(internal::ObjectStoreMutator::indexMatchesRebuild(store));
    }

    const OperationEngine engine;
    const PrepareResult result = engine.prepare(
        input, StatefulValidationContext{store, applied});

    EXPECT_EQ(result.disposition, expected_disposition);
    EXPECT_EQ(result.error.issue, expected_issue);
    EXPECT_EQ(result.plan.has_value(), expected_plan);
    EXPECT_EQ(store.allObjects(), store_before);
    expectAppliedUnchanged(applied_before, applied);
    if constexpr (std::is_same_v<Store, IndexedObjectStore>) {
        EXPECT_TRUE(internal::ObjectStoreMutator::indexMatchesRebuild(store));
    }
}

TEST(G104B10NoMutation, RejectedPreservesBothStoresAndAppliedOperationView) {
    const Operation input = operation(301U, DeleteObjectsOp{{id(99U)}});

    expectNoMutation<ReferenceObjectStore>(
        {shape(1U)},
        input,
        TestAppliedOperationView{},
        PrepareDisposition::kRejected,
        StatefulIssue::kObjectMissing,
        false);
    expectNoMutation<IndexedObjectStore>(
        {shape(1U)},
        input,
        TestAppliedOperationView{},
        PrepareDisposition::kRejected,
        StatefulIssue::kObjectMissing,
        false);
}

TEST(G104B10NoMutation, AlreadyAppliedPreservesBothStoresAndAppliedOperationView) {
    const Operation input = operation(302U, InsertObjectsOp{{shape(2U)}});
    TestAppliedOperationView applied;
    applied.entries.emplace(
        input.id, AppliedOperationEntry{input, std::nullopt});

    expectNoMutation<ReferenceObjectStore>(
        {shape(1U)},
        input,
        applied,
        PrepareDisposition::kAlreadyApplied,
        StatefulIssue::kNone,
        false);
    expectNoMutation<IndexedObjectStore>(
        {shape(1U)},
        input,
        applied,
        PrepareDisposition::kAlreadyApplied,
        StatefulIssue::kNone,
        false);
}

TEST(G104B10NoMutation, PreparedPlanIsDataOnlyAndPreservesBothStoresAndAppliedOperationView) {
    const Operation input = operation(303U, InsertObjectsOp{{shape(2U)}});

    expectNoMutation<ReferenceObjectStore>(
        {shape(1U)},
        input,
        TestAppliedOperationView{},
        PrepareDisposition::kPrepared,
        StatefulIssue::kNone,
        true);
    expectNoMutation<IndexedObjectStore>(
        {shape(1U)},
        input,
        TestAppliedOperationView{},
        PrepareDisposition::kPrepared,
        StatefulIssue::kNone,
        true);
}

} // namespace
} // namespace canvas::semantic
