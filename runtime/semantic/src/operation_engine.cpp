#include "canvas/semantic/operation_engine.hpp"

#include "canvas/semantic/applied_operation_ledger.hpp"
#include "canvas/semantic/indexed_object_store.hpp"
#include "canvas/semantic/reference_object_store.hpp"

#include "atomic_apply.hpp"
#include "change_set_builder.hpp"

#include <exception>
#include <utility>

namespace canvas::semantic {

PrepareResult OperationEngine::prepare(const Operation& operation, const StatefulValidationContext& context) const {
    return prepareApplyPlan(operation, context);
}

ApplyResult OperationEngine::apply(
    const Operation& operation,
    ApplySource source,
    ReferenceObjectStore& objects,
    AppliedOperationLedger& applied_operations,
    SemanticGenerationState& generation,
    CanonicalCommitClock& canonical_commit_clock) const {
    const PrepareResult prepared = prepareApplyPlan(
        operation, StatefulValidationContext{objects, applied_operations});
    if (prepared.disposition == PrepareDisposition::kAlreadyApplied) {
        return {ApplyDisposition::kAlreadyApplied, {}, CommitBlockReason::kNone, std::nullopt};
    }
    if (prepared.disposition == PrepareDisposition::kRejected) {
        return {ApplyDisposition::kRejected, prepared.error, CommitBlockReason::kNone, std::nullopt};
    }
    if (!prepared.plan.has_value()) {
        std::terminate();
    }

    if (canonical_commit_clock.runtimeEpoch().value() == 0U) {
        return {ApplyDisposition::kCommitBlocked, {}, CommitBlockReason::kInvalidRuntimeEpoch, std::nullopt};
    }

    const SemanticGeneration before = generation.current();
    SemanticGeneration after;
    if (!generation.prepareSuccessor(after)) {
        return {ApplyDisposition::kRejected, {}, CommitBlockReason::kNone, std::nullopt};
    }
    CommitOrdinal next_ordinal;
    if (!canonical_commit_clock.prepareSuccessor(next_ordinal)) {
        return {ApplyDisposition::kCommitBlocked, {}, CommitBlockReason::kCommitLaneExhausted, std::nullopt};
    }
    auto prepared_changes = internal::prepareChangeSet(*prepared.plan);
    if (!prepared_changes.has_value()) {
        return {ApplyDisposition::kRejected, {}, CommitBlockReason::kNone, std::nullopt};
    }

    if (internal::applyPreparedPlan(objects, *prepared.plan).status != internal::AtomicApplyStatus::kApplied) {
        return {ApplyDisposition::kRejected, {}, CommitBlockReason::kNone, std::nullopt};
    }
    if (!applied_operations.recordApplied(prepared.plan->operation)) {
        std::terminate();
    }
    generation.commitSuccessor(after);
    canonical_commit_clock.commitSuccessor(next_ordinal);
    ChangeSet change_set = internal::finalizeChangeSet(std::move(*prepared_changes), before, after);
    CanonicalCommitRecord record{
        prepared.plan->operation.id,
        source,
        before,
        after,
        CanonicalCommitStamp{canonical_commit_clock.runtimeEpoch(), next_ordinal},
        std::move(change_set)};
    return {
        ApplyDisposition::kApplied,
        {},
        CommitBlockReason::kNone,
        std::move(record)};
}

ApplyResult OperationEngine::apply(
    const Operation& operation,
    ApplySource source,
    IndexedObjectStore& objects,
    AppliedOperationLedger& applied_operations,
    SemanticGenerationState& generation,
    CanonicalCommitClock& canonical_commit_clock) const {
    const PrepareResult prepared = prepareApplyPlan(
        operation, StatefulValidationContext{objects, applied_operations});
    if (prepared.disposition == PrepareDisposition::kAlreadyApplied) {
        return {ApplyDisposition::kAlreadyApplied, {}, CommitBlockReason::kNone, std::nullopt};
    }
    if (prepared.disposition == PrepareDisposition::kRejected) {
        return {ApplyDisposition::kRejected, prepared.error, CommitBlockReason::kNone, std::nullopt};
    }
    if (!prepared.plan.has_value()) {
        std::terminate();
    }
    if (canonical_commit_clock.runtimeEpoch().value() == 0U) {
        return {ApplyDisposition::kCommitBlocked, {}, CommitBlockReason::kInvalidRuntimeEpoch, std::nullopt};
    }
    const SemanticGeneration before = generation.current();
    SemanticGeneration after;
    if (!generation.prepareSuccessor(after)) {
        return {ApplyDisposition::kRejected, {}, CommitBlockReason::kNone, std::nullopt};
    }
    CommitOrdinal next_ordinal;
    if (!canonical_commit_clock.prepareSuccessor(next_ordinal)) {
        return {ApplyDisposition::kCommitBlocked, {}, CommitBlockReason::kCommitLaneExhausted, std::nullopt};
    }
    auto prepared_changes = internal::prepareChangeSet(*prepared.plan);
    if (!prepared_changes.has_value()) {
        return {ApplyDisposition::kRejected, {}, CommitBlockReason::kNone, std::nullopt};
    }
    if (internal::applyPreparedPlan(objects, *prepared.plan).status != internal::AtomicApplyStatus::kApplied) {
        return {ApplyDisposition::kRejected, {}, CommitBlockReason::kNone, std::nullopt};
    }
    if (!applied_operations.recordApplied(prepared.plan->operation)) {
        std::terminate();
    }
    generation.commitSuccessor(after);
    canonical_commit_clock.commitSuccessor(next_ordinal);
    ChangeSet change_set = internal::finalizeChangeSet(std::move(*prepared_changes), before, after);
    CanonicalCommitRecord record{
        prepared.plan->operation.id,
        source,
        before,
        after,
        CanonicalCommitStamp{canonical_commit_clock.runtimeEpoch(), next_ordinal},
        std::move(change_set)};
    return {ApplyDisposition::kApplied, {}, CommitBlockReason::kNone, std::move(record)};
}

} // namespace canvas::semantic
