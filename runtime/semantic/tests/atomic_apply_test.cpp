#include "canvas/semantic/indexed_object_store.hpp"
#include "canvas/semantic/reference_object_store.hpp"

#include "atomic_apply.hpp"
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

template <typename Store>
void seed(Store& store, const std::vector<ObjectRecord>& records) {
    for (const ObjectRecord& record : records) {
        ASSERT_TRUE(internal::ObjectStoreMutator::insertFresh(store, record));
    }
}

template <typename Store>
void expectPreconditionFailureWithoutMutation(
    Store& store,
    const PreparedApplyPlan& plan,
    const std::vector<ObjectRecord>& expected_before) {
    EXPECT_EQ(store.allObjects(), expected_before);
    const internal::AtomicApplyResult result = internal::applyPreparedPlan(store, plan);
    EXPECT_EQ(result.status, internal::AtomicApplyStatus::kPreconditionFailed);
    EXPECT_EQ(store.allObjects(), expected_before);
    if constexpr (std::is_same_v<Store, IndexedObjectStore>) {
        EXPECT_TRUE(internal::ObjectStoreMutator::indexMatchesRebuild(store));
    }
}

template <typename PlanFactory>
void expectBothProvidersRejectWithoutMutation(PlanFactory&& make_plan) {
    const std::vector<ObjectRecord> initial = {shape(2U, 2U), shape(3U, 3U)};
    ReferenceObjectStore reference;
    IndexedObjectStore indexed;
    seed(reference, initial);
    seed(indexed, initial);
    expectPreconditionFailureWithoutMutation(reference, make_plan(), initial);
    expectPreconditionFailureWithoutMutation(indexed, make_plan(), initial);
}

} // namespace

TEST(AtomicApply, AppliesPreparedMixedPlanToBothStoresWithExplicitFinalState) {
    const ObjectRecord original = shape(2U, 2U);
    const ObjectRecord deleted = shape(3U, 3U);
    const ObjectRecord untouched = shape(4U, 4U);
    const ObjectRecord replacement = shape(2U, 8U);
    const ObjectRecord created = shape(1U, 1U);
    const std::vector<ObjectRecord> initial = {original, deleted, untouched};
    const std::vector<ObjectRecord> expected_final = {created, replacement, untouched};
    const PreparedApplyPlan plan{
        .creates = {created}, .replacements = {replacement}, .deletes = {deleted.id}, .delete_closure = std::nullopt};

    ReferenceObjectStore reference;
    IndexedObjectStore indexed;
    seed(reference, initial);
    seed(indexed, initial);

    EXPECT_EQ(internal::applyPreparedPlan(reference, plan).status, internal::AtomicApplyStatus::kApplied);
    EXPECT_EQ(reference.allObjects(), expected_final);

    EXPECT_EQ(internal::applyPreparedPlan(indexed, plan).status, internal::AtomicApplyStatus::kApplied);
    EXPECT_EQ(indexed.allObjects(), expected_final);
    EXPECT_TRUE(internal::ObjectStoreMutator::indexMatchesRebuild(indexed));
}

TEST(AtomicApply, RejectsExistingCreateTargetBeforeAnyEarlierMutation) {
    expectBothProvidersRejectWithoutMutation([] {
        PreparedApplyPlan plan{};
        plan.creates = {shape(1U, 1U), shape(2U, 9U)};
        return plan;
    });
}

TEST(AtomicApply, RejectsDuplicateCreateIdBeforeAnyEarlierMutation) {
    expectBothProvidersRejectWithoutMutation([] {
        PreparedApplyPlan plan{};
        plan.creates = {shape(1U, 1U), shape(1U, 9U)};
        return plan;
    });
}

TEST(AtomicApply, RejectsMissingReplacementTargetBeforeAnyEarlierMutation) {
    expectBothProvidersRejectWithoutMutation([] {
        PreparedApplyPlan plan{};
        plan.creates = {shape(1U, 1U)};
        plan.replacements = {shape(99U, 9U)};
        return plan;
    });
}

TEST(AtomicApply, RejectsDuplicateReplacementIdBeforeAnyEarlierMutation) {
    expectBothProvidersRejectWithoutMutation([] {
        PreparedApplyPlan plan{};
        plan.creates = {shape(1U, 1U)};
        plan.replacements = {shape(2U, 8U), shape(2U, 9U)};
        return plan;
    });
}

TEST(AtomicApply, RejectsMissingDeleteTargetBeforeAnyEarlierMutation) {
    expectBothProvidersRejectWithoutMutation([] {
        PreparedApplyPlan plan{};
        plan.creates = {shape(1U, 1U)};
        plan.replacements = {shape(2U, 8U)};
        plan.deletes = {id(99U)};
        return plan;
    });
}

TEST(AtomicApply, RejectsDuplicateDeleteIdBeforeAnyEarlierMutation) {
    expectBothProvidersRejectWithoutMutation([] {
        PreparedApplyPlan plan{};
        plan.creates = {shape(1U, 1U)};
        plan.replacements = {shape(2U, 8U)};
        plan.deletes = {id(3U), id(3U)};
        return plan;
    });
}

TEST(AtomicApply, RejectsContradictoryCreateAndReplacementIdBeforeAnyMutation) {
    expectBothProvidersRejectWithoutMutation([] {
        PreparedApplyPlan plan{};
        plan.creates = {shape(1U, 1U), shape(2U, 8U)};
        plan.replacements = {shape(2U, 9U)};
        return plan;
    });
}

TEST(AtomicApply, RejectsContradictoryCreateAndDeleteIdBeforeAnyMutation) {
    expectBothProvidersRejectWithoutMutation([] {
        PreparedApplyPlan plan{};
        plan.creates = {shape(1U, 1U)};
        plan.deletes = {id(1U)};
        return plan;
    });
}

TEST(AtomicApply, RejectsContradictoryReplacementAndDeleteIdBeforeAnyMutation) {
    expectBothProvidersRejectWithoutMutation([] {
        PreparedApplyPlan plan{};
        plan.creates = {shape(1U, 1U)};
        plan.replacements = {shape(2U, 8U)};
        plan.deletes = {id(2U)};
        return plan;
    });
}

} // namespace canvas::semantic
