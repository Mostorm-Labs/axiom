#include "g1_08_verifier.hpp"

#include "canvas/semantic/indexed_object_store.hpp"
#include "canvas/semantic/reference_object_store.hpp"
#include "canvas/semantic/object_record.hpp"
#include "object_store_mutator.hpp"
#include "canvas/semantic/applied_operation_ledger.hpp"
#include "canvas/semantic/canonical_commit_clock.hpp"
#include "canvas/semantic/operation_engine.hpp"

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
    const auto scale_one = runIndexedLocalityWorkload(1000U);
    const auto scale_two = runIndexedLocalityWorkload(10000U);
    const auto scale_three = runIndexedLocalityWorkload(100000U);
    result.scales_checked = 3U;
    result.locality = scale_three;
    const auto locality_ok = [](const LocalityWorkloadResult& workload) {
        return workload.access.find_calls == 1U &&
               workload.access.all_objects_calls == 0U &&
               workload.access.all_objects_records_materialized == 0U;
    };
    result.locality_pass = locality_ok(scale_one) && locality_ok(scale_two) &&
                          locality_ok(scale_three);
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
