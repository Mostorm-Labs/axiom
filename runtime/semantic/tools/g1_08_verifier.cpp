#include "g1_08_verifier.hpp"

#include "canvas/semantic/indexed_object_store.hpp"
#include "canvas/semantic/reference_object_store.hpp"
#include "canvas/semantic/object_record.hpp"
#include "object_store_mutator.hpp"
#include "canvas/semantic/applied_operation_ledger.hpp"
#include "canvas/semantic/canonical_commit_clock.hpp"
#include "canvas/semantic/operation_engine.hpp"
#include <algorithm>
#include <array>

namespace canvas::verification::g1_08 {
namespace {
canvas::semantic::ObjectRecord record(std::uint64_t value) {
    using namespace canvas::semantic;
    ObjectRecord result{};
    result.id = ObjectId::fromUint64(value);
    result.kind = ObjectKind::kShape;
    result.kind_version = 1U;
    result.placement = Placement{std::nullopt, OrderKey({1U})};
    result.transform = Transform2D{1.0, 0.0, 0.0, 1.0, 0.0, 0.0};
    result.content = ShapeContent{1U, 1.0, 1.0};
    return result;
}
} // namespace

VerificationSummary verifyReferenceIndexedAndLocality() {
    canvas::semantic::ReferenceObjectStore reference;
    canvas::semantic::IndexedObjectStore indexed;
    for (std::uint64_t i = 1U; i <= 8U; ++i) {
        const auto value = record(i);
        static_cast<void>(canvas::semantic::internal::ObjectStoreMutator::insertFresh(reference, value));
        static_cast<void>(canvas::semantic::internal::ObjectStoreMutator::insertFresh(indexed, value));
    }
    VerificationSummary result;
    result.correctness_pass = reference.allObjects() == indexed.allObjects();
    const std::array<std::size_t, 3> scales{1000U, 10000U, 100000U};
    for (std::size_t i = 0U; i < scales.size(); ++i) {
        result.local_mutation[i] = runIndexedLocalityWorkload(scales[i]);
        result.hierarchy[i] = runHierarchyWorkload(scales[i]);
        result.connector_delete[i] = runConnectorDeleteWorkload(scales[i]);
    }
    result.controlled_cascade = {runControlledCascadeWorkload(8U),
                                 runControlledCascadeWorkload(64U),
                                 runControlledCascadeWorkload(512U)};
    result.scales_checked = 3U;
    result.locality = result.local_mutation.back();
    const auto sameShape = [](const auto& left, const auto& right) {
        return left.access.find_calls == right.access.find_calls &&
               left.access.contains_calls == right.access.contains_calls &&
               left.access.children_calls == right.access.children_calls &&
               left.access.children_records_materialized == right.access.children_records_materialized &&
               left.access.insert_fresh_calls == right.access.insert_fresh_calls &&
               left.access.replace_existing_calls == right.access.replace_existing_calls &&
               left.access.erase_existing_calls == right.access.erase_existing_calls &&
               left.access.index_rebuild_check_calls == right.access.index_rebuild_check_calls;
    };
    result.local_mutation_access_shape_pass = sameShape(result.local_mutation.front(), result.local_mutation.back());
    result.hierarchy_access_shape_pass = sameShape(result.hierarchy.front(), result.hierarchy.back());
    result.cascade_access_shape_pass =
        result.controlled_cascade[0].access.erase_existing_calls < result.controlled_cascade[1].access.erase_existing_calls &&
        result.controlled_cascade[1].access.erase_existing_calls < result.controlled_cascade[2].access.erase_existing_calls &&
        result.controlled_cascade[0].access.all_objects_calls == result.controlled_cascade[1].access.all_objects_calls &&
        result.controlled_cascade[1].access.all_objects_calls == result.controlled_cascade[2].access.all_objects_calls &&
        result.controlled_cascade[0].access.all_objects_records_materialized == result.controlled_cascade[1].access.all_objects_records_materialized &&
        result.controlled_cascade[1].access.all_objects_records_materialized == result.controlled_cascade[2].access.all_objects_records_materialized;
    const auto locality_ok = [](const LocalityWorkloadResult& workload) {
        return workload.applied &&
               workload.access.all_objects_calls == 0U &&
               workload.access.all_objects_records_materialized == 0U &&
               workload.access.index_rebuild_check_calls == 0U;
    };
    const auto apply_ok = [](const LocalityWorkloadResult& workload) {
        return workload.applied && workload.access.all_objects_calls == 0U &&
               workload.access.all_objects_records_materialized == 0U &&
               workload.access.index_rebuild_check_calls == 0U;
    };
    result.locality_pass = result.local_mutation_access_shape_pass &&
                          result.hierarchy_access_shape_pass &&
                          result.cascade_access_shape_pass &&
                          std::all_of(result.local_mutation.begin(), result.local_mutation.end(), locality_ok) &&
                          std::all_of(result.hierarchy.begin(), result.hierarchy.end(), apply_ok) &&
                          std::all_of(result.connector_delete.begin(), result.connector_delete.end(), apply_ok) &&
                          std::all_of(result.controlled_cascade.begin(), result.controlled_cascade.end(), apply_ok);
    canvas::semantic::AppliedOperationLedger ledger;
    canvas::semantic::SemanticGenerationState generation;
    canvas::semantic::CanonicalCommitClock clock(canvas::semantic::RuntimeEpoch(42U));
    canvas::semantic::Operation operation{};
    operation.id = canvas::semantic::OperationId{canvas::semantic::ObjectId::fromUint64(9000U)};
    operation.document_id = canvas::semantic::DocumentId{canvas::semantic::ObjectId::fromUint64(99U)};
    operation.schema_version = 1U;
    operation.payload_version = 1U;
    operation.payload = canvas::semantic::DeleteObjectsOp{{canvas::semantic::ObjectId::fromUint64(1U)}};
    canvas::semantic::IndexedObjectStore measured_store;
    for (std::uint64_t i = 1U; i <= 1000U; ++i) {
        static_cast<void>(canvas::semantic::internal::ObjectStoreMutator::insertFresh(measured_store, record(i)));
    }
    canvas::semantic::internal::resetIndexedAccessProbe();
    canvas::semantic::internal::enableIndexedAccessProbe(true);
    static_cast<void>(canvas::semantic::OperationEngine{}.apply(
        operation, canvas::semantic::ApplySource::kLocalInteraction, measured_store,
        ledger, generation, clock));
    const auto delete_probe = canvas::semantic::internal::snapshotIndexedAccessProbe();
    canvas::semantic::internal::enableIndexedAccessProbe(false);
    result.delete_reverse_scan_observed = delete_probe.all_objects_calls > 0U &&
                                           delete_probe.all_objects_records_materialized == 1000U;
    result.locality_pass = result.locality_pass && !result.delete_reverse_scan_observed;
    return result;
}

} // namespace canvas::verification::g1_08
