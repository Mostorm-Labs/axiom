#include "canvas/semantic/indexed_object_store.hpp"
#include "g1_08_indexed_access_probe_internal.hpp"
#include "object_store_mutator.hpp"
#include "canvas/semantic/applied_operation_ledger.hpp"
#include "canvas/semantic/canonical_commit_clock.hpp"
#include "canvas/semantic/operation_engine.hpp"

#include <gtest/gtest.h>

#include <cstdint>

namespace canvas::semantic {
namespace {

ObjectRecord record(std::uint64_t value) {
    ObjectRecord result{};
    result.id = ObjectId::fromUint64(value);
    result.kind = ObjectKind::kShape;
    result.kind_version = 1U;
    result.placement = Placement{std::nullopt, OrderKey({1U})};
    result.transform = Transform2D{1.0, 0.0, 0.0, 1.0, 0.0, 0.0};
    result.content = ShapeContent{1U, 10.0, 20.0};
    return result;
}

} // namespace

TEST(G108Locality, ProbeIsDisabledByDefaultAndCountsOnlyExplicitMeasurements) {
    IndexedObjectStore store;
    ASSERT_TRUE(internal::ObjectStoreMutator::insertFresh(store, record(1U)));
    static_cast<void>(store.find(ObjectId::fromUint64(1U)));
    static_cast<void>(store.allObjects());
    const auto disabled = internal::snapshotIndexedAccessProbe();
    EXPECT_EQ(disabled.find_calls, 0U);
    EXPECT_EQ(disabled.all_objects_calls, 0U);

    internal::resetIndexedAccessProbe();
    internal::enableIndexedAccessProbe(true);
    static_cast<void>(store.find(ObjectId::fromUint64(1U)));
    const auto enabled = internal::snapshotIndexedAccessProbe();
    internal::enableIndexedAccessProbe(false);
    EXPECT_EQ(enabled.find_calls, 1U);
    EXPECT_EQ(enabled.all_objects_calls, 0U);
}

TEST(G108Locality, IndexedLocalReadDoesNotMaterializeWholeStore) {
    IndexedObjectStore store;
    for (std::uint64_t value = 1U; value <= 100U; ++value) {
        ASSERT_TRUE(internal::ObjectStoreMutator::insertFresh(store, record(value)));
    }
    internal::resetIndexedAccessProbe();
    internal::enableIndexedAccessProbe(true);
    ASSERT_NE(store.find(ObjectId::fromUint64(50U)), nullptr);
    const auto measured = internal::snapshotIndexedAccessProbe();
    internal::enableIndexedAccessProbe(false);
    EXPECT_EQ(measured.find_calls, 1U);
    EXPECT_EQ(measured.all_objects_calls, 0U);
    EXPECT_EQ(measured.all_objects_records_materialized, 0U);
}

TEST(G108Locality, ControlledProbeRecordsExpectedDeleteReverseScanRisk) {
    IndexedObjectStore store;
    for (std::uint64_t value = 1U; value <= 1000U; ++value) {
        ASSERT_TRUE(internal::ObjectStoreMutator::insertFresh(store, record(value)));
    }
    AppliedOperationLedger ledger;
    SemanticGenerationState generation;
    CanonicalCommitClock clock(RuntimeEpoch(42U));
    Operation operation{};
    operation.id = OperationId{ObjectId::fromUint64(9000U)};
    operation.document_id = DocumentId{ObjectId::fromUint64(99U)};
    operation.schema_version = 1U;
    operation.payload_version = 1U;
    operation.payload = DeleteObjectsOp{{ObjectId::fromUint64(1U)}};
    internal::resetIndexedAccessProbe();
    internal::enableIndexedAccessProbe(true);
    const auto result = OperationEngine{}.apply(operation, ApplySource::kLocalInteraction,
                                                store, ledger, generation, clock);
    const auto measured = internal::snapshotIndexedAccessProbe();
    internal::enableIndexedAccessProbe(false);
    ASSERT_EQ(result.disposition, ApplyDisposition::kApplied);
    EXPECT_GT(measured.all_objects_calls, 0U);
    EXPECT_EQ(measured.all_objects_records_materialized, 1000U);
}

} // namespace canvas::semantic
