#include "canvas/semantic/indexed_object_store.hpp"
#include "canvas/semantic/reference_object_store.hpp"

#include "atomic_apply.hpp"
#include "object_store_mutator.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <fstream>
#include <iterator>
#include <optional>
#include <string>
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

} // namespace

TEST(AtomicApplyDifferential, ValidPlanMatchesExplicitExpectedProjectionForBothProviders) {
    const std::vector<ObjectRecord> initial = {shape(2U, 2U), shape(3U, 3U)};
    const std::vector<ObjectRecord> expected = {shape(1U, 1U), shape(2U, 7U)};
    const PreparedApplyPlan plan{
        .creates = {shape(1U, 1U)}, .replacements = {shape(2U, 7U)}, .deletes = {id(3U)}};
    ReferenceObjectStore reference;
    IndexedObjectStore indexed;
    seed(reference, initial);
    seed(indexed, initial);

    const internal::AtomicApplyResult reference_result = internal::applyPreparedPlan(reference, plan);
    const internal::AtomicApplyResult indexed_result = internal::applyPreparedPlan(indexed, plan);

    EXPECT_EQ(reference_result.status, internal::AtomicApplyStatus::kApplied);
    EXPECT_EQ(indexed_result.status, internal::AtomicApplyStatus::kApplied);
    EXPECT_EQ(reference.allObjects(), expected);
    EXPECT_EQ(indexed.allObjects(), expected);
    EXPECT_TRUE(internal::ObjectStoreMutator::indexMatchesRebuild(indexed));
}

TEST(AtomicApplyDifferential, InvalidPlanPreservesExplicitInitialProjectionForBothProviders) {
    const std::vector<ObjectRecord> initial = {shape(2U, 2U), shape(3U, 3U)};
    const PreparedApplyPlan plan{
        .creates = {shape(1U, 1U)}, .replacements = {shape(2U, 7U)}, .deletes = {id(99U)}};
    ReferenceObjectStore reference;
    IndexedObjectStore indexed;
    seed(reference, initial);
    seed(indexed, initial);

    const internal::AtomicApplyResult reference_result = internal::applyPreparedPlan(reference, plan);
    const internal::AtomicApplyResult indexed_result = internal::applyPreparedPlan(indexed, plan);

    EXPECT_EQ(reference_result.status, internal::AtomicApplyStatus::kPreconditionFailed);
    EXPECT_EQ(indexed_result.status, internal::AtomicApplyStatus::kPreconditionFailed);
    EXPECT_EQ(reference.allObjects(), initial);
    EXPECT_EQ(indexed.allObjects(), initial);
    EXPECT_TRUE(internal::ObjectStoreMutator::indexMatchesRebuild(indexed));
}

TEST(AtomicApplyDifferential, IndexedProductionApplyDoesNotScanAllObjects) {
    std::ifstream source(ATOMIC_APPLY_SOURCE_PATH);
    ASSERT_TRUE(source.is_open());
    const std::string contents{std::istreambuf_iterator<char>(source), std::istreambuf_iterator<char>()};
    EXPECT_EQ(contents.find("allObjects("), std::string::npos);
}

} // namespace canvas::semantic
