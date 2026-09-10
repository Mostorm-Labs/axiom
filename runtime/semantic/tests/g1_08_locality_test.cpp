#include "canvas/semantic/indexed_object_store.hpp"
#include "g1_08_indexed_access_probe_internal.hpp"
#include "object_store_mutator.hpp"
#include "canvas/semantic/applied_operation_ledger.hpp"
#include "canvas/semantic/canonical_commit_clock.hpp"
#include "canvas/semantic/operation_engine.hpp"
#include "canvas/semantic/object_content.hpp"
#include "../tools/g1_08_locality_workload.hpp"
#include "../tools/g1_08_verifier.hpp"

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

ObjectRecord connector(std::uint64_t value, ObjectId target) {
    ObjectRecord result{};
    result.id = ObjectId::fromUint64(value);
    result.kind = ObjectKind::kConnector;
    result.kind_version = 1U;
    result.placement = Placement{std::nullopt, OrderKey({1U})};
    ConnectorContent content{};
    content.start.value = AttachedEndpoint{target, AutoPerimeterAnchor{}};
    content.end.value = FreePointEndpoint{Vec2{1.0, 1.0}};
    content.routing = ConnectorRouting::kStraight;
    result.content = content;
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

TEST(G108Locality, DeleteClosureUsesIndexedReverseLookupWithoutFullStoreScan) {
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
    EXPECT_EQ(measured.all_objects_calls, 0U);
    EXPECT_EQ(measured.all_objects_records_materialized, 0U);
}

TEST(G108Locality, IndexedReverseLookupScalesWithoutMaterializingUnrelatedRecords) {
    IndexedObjectStore store;
    for (std::uint64_t value = 1U; value <= 1000U; ++value) {
        ASSERT_TRUE(internal::ObjectStoreMutator::insertFresh(store, record(value)));
    }
    AppliedOperationLedger ledger;
    SemanticGenerationState generation;
    CanonicalCommitClock clock(RuntimeEpoch(43U));
    Operation operation{};
    operation.id = OperationId{ObjectId::fromUint64(9001U)};
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
    EXPECT_EQ(measured.all_objects_calls, 0U);
    EXPECT_EQ(measured.all_objects_records_materialized, 0U);
}

TEST(G108Locality, T08L03AllRequiredScalesRemainScanFree) {
    for (const std::size_t total : {1000U, 10000U, 100000U}) {
        const auto measured = verification::g1_08::runIndexedLocalityWorkload(total);
        EXPECT_EQ(measured.total_objects, total);
        EXPECT_EQ(measured.access.find_calls, 8U);
        EXPECT_EQ(measured.access.all_objects_calls, 0U);
        EXPECT_EQ(measured.access.all_objects_records_materialized, 0U);
    }
}

TEST(G108Locality, FrozenLocalMutationUsesOperationEngineAtAllScales) {
    for (const std::size_t total : {1000U, 10000U, 100000U}) {
        const auto measured = verification::g1_08::runIndexedLocalityWorkload(total);
        EXPECT_STREQ(measured.workload, "W08-LOCAL-MUTATION");
        EXPECT_TRUE(measured.applied);
        EXPECT_EQ(measured.affected_objects, 8U);
        EXPECT_EQ(measured.access.all_objects_calls, 0U);
        EXPECT_EQ(measured.access.all_objects_records_materialized, 0U);
        EXPECT_EQ(measured.access.index_rebuild_check_calls, 0U);
    }
}

TEST(G108Locality, AccessShapeForLocalMutationIsScaleInvariant) {
    const auto small = verification::g1_08::runIndexedLocalityWorkload(1000U).access;
    const auto large = verification::g1_08::runIndexedLocalityWorkload(100000U).access;
    EXPECT_EQ(small.find_calls, large.find_calls);
    EXPECT_EQ(small.contains_calls, large.contains_calls);
    EXPECT_EQ(small.children_calls, large.children_calls);
    EXPECT_EQ(small.insert_fresh_calls, large.insert_fresh_calls);
    EXPECT_EQ(small.replace_existing_calls, large.replace_existing_calls);
    EXPECT_EQ(small.erase_existing_calls, large.erase_existing_calls);
}

TEST(G108Locality, AccessShapeEqualityCoversEveryFrozenProbeObservation) {
    internal::IndexedAccessProbeSnapshot expected{};
    expected.all_objects_calls = 1U;
    expected.all_objects_records_materialized = 2U;
    expected.find_calls = 3U;
    expected.contains_calls = 4U;
    expected.children_calls = 5U;
    expected.children_records_materialized = 6U;
    expected.insert_fresh_calls = 7U;
    expected.replace_existing_calls = 8U;
    expected.erase_existing_calls = 9U;
    expected.index_rebuild_check_calls = 10U;
    EXPECT_TRUE(verification::g1_08::sameIndexedAccessShape(expected, expected));
    for (const auto member : {
             &internal::IndexedAccessProbeSnapshot::all_objects_calls,
             &internal::IndexedAccessProbeSnapshot::all_objects_records_materialized,
             &internal::IndexedAccessProbeSnapshot::find_calls,
             &internal::IndexedAccessProbeSnapshot::contains_calls,
             &internal::IndexedAccessProbeSnapshot::children_calls,
             &internal::IndexedAccessProbeSnapshot::children_records_materialized,
             &internal::IndexedAccessProbeSnapshot::insert_fresh_calls,
             &internal::IndexedAccessProbeSnapshot::replace_existing_calls,
             &internal::IndexedAccessProbeSnapshot::erase_existing_calls,
             &internal::IndexedAccessProbeSnapshot::index_rebuild_check_calls}) {
        auto changed = expected;
        ++(changed.*member);
        EXPECT_FALSE(verification::g1_08::sameIndexedAccessShape(expected, changed));
    }
}

TEST(G108Locality, FrozenHierarchyUsesBoundedHotTopologyAtAllScales) {
    for (const std::size_t total : {1000U, 10000U, 100000U}) {
        const auto measured = verification::g1_08::runHierarchyWorkload(total);
        EXPECT_STREQ(measured.workload, "W08-HIERARCHY");
        EXPECT_TRUE(measured.applied);
        EXPECT_EQ(measured.affected_objects, 8U);
        EXPECT_EQ(measured.access.all_objects_calls, 0U);
        EXPECT_EQ(measured.access.all_objects_records_materialized, 0U);
    }
}

TEST(G108Locality, AccessShapeForHierarchyIsIndependentOfColdPopulation) {
    const auto small = verification::g1_08::runHierarchyWorkload(1000U).access;
    const auto large = verification::g1_08::runHierarchyWorkload(100000U).access;
    EXPECT_EQ(small.find_calls, large.find_calls);
    EXPECT_EQ(small.contains_calls, large.contains_calls);
    EXPECT_EQ(small.children_calls, large.children_calls);
    EXPECT_EQ(small.insert_fresh_calls, large.insert_fresh_calls);
    EXPECT_EQ(small.replace_existing_calls, large.replace_existing_calls);
    EXPECT_EQ(small.erase_existing_calls, large.erase_existing_calls);
}

TEST(G108Locality, FrozenConnectorDeleteUsesBoundedReverseRelationAtAllScales) {
    for (const std::size_t total : {1000U, 10000U, 100000U}) {
        const auto measured = verification::g1_08::runConnectorDeleteWorkload(total);
        EXPECT_STREQ(measured.workload, "W08-CONNECTOR-DELETE");
        EXPECT_TRUE(measured.applied);
        EXPECT_EQ(measured.affected_objects, 8U);
        EXPECT_EQ(measured.access.all_objects_calls, 0U);
        EXPECT_EQ(measured.access.all_objects_records_materialized, 0U);
    }
}

TEST(G108Locality, FrozenControlledCascadeScalesWithAffectedClosure) {
    for (const std::size_t connectors : {8U, 64U, 512U}) {
        const auto measured = verification::g1_08::runControlledCascadeWorkload(connectors);
        EXPECT_STREQ(measured.workload, "W08-CONTROLLED-CASCADE");
        EXPECT_TRUE(measured.applied);
        EXPECT_EQ(measured.total_objects, 100000U);
        EXPECT_EQ(measured.affected_objects, connectors);
        EXPECT_EQ(measured.access.all_objects_calls, 0U);
        EXPECT_EQ(measured.access.all_objects_records_materialized, 0U);
    }
}

TEST(G108Locality, AccessShapeForCascadeTracksTrueClosure) {
    const auto closure8 = verification::g1_08::runControlledCascadeWorkload(8U).access;
    const auto closure64 = verification::g1_08::runControlledCascadeWorkload(64U).access;
    const auto closure512 = verification::g1_08::runControlledCascadeWorkload(512U).access;
    EXPECT_LT(closure8.erase_existing_calls, closure64.erase_existing_calls);
    EXPECT_LT(closure64.erase_existing_calls, closure512.erase_existing_calls);
    EXPECT_EQ(closure8.all_objects_calls, closure64.all_objects_calls);
    EXPECT_EQ(closure64.all_objects_calls, closure512.all_objects_calls);
    EXPECT_EQ(closure8.all_objects_records_materialized, closure64.all_objects_records_materialized);
    EXPECT_EQ(closure64.all_objects_records_materialized, closure512.all_objects_records_materialized);
}

TEST(G108Locality, CascadeStructuralOracleRejectsHiddenColdPopulationWorkInEveryCounter) {
    auto workload = verification::g1_08::runControlledCascadeWorkload(8U);
    ASSERT_TRUE(verification::g1_08::controlledCascadeAccessShapeMatchesClosure(workload));
    for (const auto member : {
             &internal::IndexedAccessProbeSnapshot::all_objects_calls,
             &internal::IndexedAccessProbeSnapshot::all_objects_records_materialized,
             &internal::IndexedAccessProbeSnapshot::find_calls,
             &internal::IndexedAccessProbeSnapshot::contains_calls,
             &internal::IndexedAccessProbeSnapshot::children_calls,
             &internal::IndexedAccessProbeSnapshot::children_records_materialized,
             &internal::IndexedAccessProbeSnapshot::insert_fresh_calls,
             &internal::IndexedAccessProbeSnapshot::replace_existing_calls,
             &internal::IndexedAccessProbeSnapshot::erase_existing_calls,
             &internal::IndexedAccessProbeSnapshot::index_rebuild_check_calls}) {
        auto hidden_work = workload;
        ++(hidden_work.access.*member);
        EXPECT_FALSE(verification::g1_08::controlledCascadeAccessShapeMatchesClosure(hidden_work));
    }
}

TEST(G108Locality, T08L04ControlledCascadesUseOnlyAffectedClosure) {
    for (const std::uint64_t connector_count : {8U, 64U, 512U}) {
        IndexedObjectStore store;
        constexpr std::uint64_t target_id = 1U;
        ASSERT_TRUE(internal::ObjectStoreMutator::insertFresh(store, record(target_id)));
        for (std::uint64_t i = 0U; i < connector_count; ++i) {
            ASSERT_TRUE(internal::ObjectStoreMutator::insertFresh(
                store, connector(1000U + i, ObjectId::fromUint64(target_id))));
        }
        AppliedOperationLedger ledger;
        SemanticGenerationState generation;
        CanonicalCommitClock clock(RuntimeEpoch(44U));
        Operation operation{};
        operation.id = OperationId{ObjectId::fromUint64(90000U + connector_count)};
        operation.document_id = DocumentId{ObjectId::fromUint64(99U)};
        operation.schema_version = 1U;
        operation.payload_version = 1U;
        operation.payload = DeleteObjectsOp{{ObjectId::fromUint64(target_id)}};
        internal::resetIndexedAccessProbe();
        internal::enableIndexedAccessProbe(true);
        const auto result = OperationEngine{}.apply(operation, ApplySource::kLocalInteraction,
                                                    store, ledger, generation, clock);
        const auto measured = internal::snapshotIndexedAccessProbe();
        internal::enableIndexedAccessProbe(false);
        ASSERT_EQ(result.disposition, ApplyDisposition::kApplied);
        EXPECT_EQ(measured.all_objects_calls, 0U);
        EXPECT_EQ(measured.all_objects_records_materialized, 0U);
        EXPECT_EQ(store.size(), 0U);
    }
}

} // namespace canvas::semantic
