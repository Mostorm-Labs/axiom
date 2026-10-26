#include "canvas/semantic/operation_engine.hpp"

#include "canvas/semantic/applied_operation_ledger.hpp"
#include "canvas/semantic/indexed_object_store.hpp"
#include "canvas/semantic/reference_object_store.hpp"
#include "canvas/semantic/normalizer.hpp"
#include "canvas/semantic/validator.hpp"

#include "atomic_apply.hpp"
#include "change_set_builder.hpp"

#include <exception>
#include <new>
#include <utility>

#if defined(AXIOM_FC08_QUALIFICATION_HOOKS)
#include "qualification_checkpoint.hpp"
#define FC08_ENGINE_SCOPE \
    if (canvas::qualification::observing(fc08Qualification_)) return {}; \
    canvas::qualification::FaultScope fc08Scope{fc08Qualification_.get()}; \
    canvas::qualification::ApplyContext fc08Context{fc08Qualification_.get(), this, \
        generation.current().value(), canonical_commit_clock.lastCommittedOrdinal().value(), \
        canvas::qualification::kApplyPlan};
#define FC08_ENGINE_STAGED \
    if (canvas::qualification::notify(&fc08Context, canvas::qualification::Point::kSemStaged, prepared.plan->operation)) \
        return {ApplyDisposition::kRejected, {}, CommitBlockReason::kNone, std::nullopt};
#define FC08_ENGINE_RESERVED \
    fc08Context.resources |= canvas::qualification::kChangeSet; \
    if (canvas::qualification::notify(&fc08Context, canvas::qualification::Point::kSemReserved, prepared.plan->operation)) \
        return {ApplyDisposition::kRejected, {}, CommitBlockReason::kNone, std::nullopt};
#define FC08_ENGINE_DONE \
    fc08Context.generation = generation.current().value(); \
    fc08Context.ordinal = canonical_commit_clock.lastCommittedOrdinal().value(); \
    fc08Context.resources |= canvas::qualification::kPublished; \
    (void)canvas::qualification::notify(&fc08Context, canvas::qualification::Point::kSemDone, prepared.plan->operation);
#define FC08_ATOMIC_ARGUMENT , &fc08Context
#else
#define FC08_ENGINE_SCOPE
#define FC08_ENGINE_STAGED
#define FC08_ENGINE_RESERVED
#define FC08_ENGINE_DONE
#define FC08_ATOMIC_ARGUMENT
#endif


namespace canvas::semantic {
PrepareResult OperationEngine::prepare(const Operation& operation, const StatefulValidationContext& context) const {
    // Typed/direct/replay mutation uses the same envelope, normalization and
    // structural admission before the stateful planner. There is no V1 bypass.
    if (!validateEnvelope(operation, OperationFieldPresence{true, true}).ok()) {
        return {PrepareDisposition::kRejected, {StatefulIssue::kInvalidApplicability}, std::nullopt};
    }
    const NormalizeResult normalized = normalizeOperation(operation);
    if (!normalized.ok() || !validatePayloadStructure(normalized.value).ok()) {
        return {PrepareDisposition::kRejected, {StatefulIssue::kInvalidApplicability}, std::nullopt};
    }
    if ((operation.kind() == OperationKind::kSetSectionContent || operation.kind() == OperationKind::kEditContainerTopology) &&
        !SemanticCodec::encodeProtobufOperation(normalized.value).ok())
        return {PrepareDisposition::kRejected, {StatefulIssue::kInvalidApplicability}, std::nullopt};
    return prepareApplyPlan(normalized.value, context);
}

ApplyResult OperationEngine::apply(
    const Operation& operation,
    ApplySource source,
    ReferenceObjectStore& objects,
    AppliedOperationLedger& applied_operations,
    SemanticGenerationState& generation,
    CanonicalCommitClock& canonical_commit_clock) const try {
    FC08_ENGINE_SCOPE
    const PrepareResult prepared = prepare(
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

    FC08_ENGINE_STAGED
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

    auto reserved_ledger = applied_operations.reserveContainerEntry(prepared.plan->operation);
    FC08_ENGINE_RESERVED
    if (internal::applyPreparedPlan(objects, *prepared.plan FC08_ATOMIC_ARGUMENT).status != internal::AtomicApplyStatus::kApplied) {
        return {ApplyDisposition::kRejected, {}, CommitBlockReason::kNone, std::nullopt};
    }
    if (!applied_operations.recordReservedOrApplied(prepared.plan->operation, std::move(reserved_ledger))) {
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
    FC08_ENGINE_DONE
    return {
        ApplyDisposition::kApplied,
        {},
        CommitBlockReason::kNone,
        std::move(record)};
} catch (const std::bad_alloc&) {
    if (operation.kind() == OperationKind::kSetSectionContent || operation.kind() == OperationKind::kEditContainerTopology)
        return {ApplyDisposition::kRejected, {}, CommitBlockReason::kNone, std::nullopt};
    throw;
}

ApplyResult OperationEngine::apply(
    const Operation& operation,
    ApplySource source,
    IndexedObjectStore& objects,
    AppliedOperationLedger& applied_operations,
    SemanticGenerationState& generation,
    CanonicalCommitClock& canonical_commit_clock) const try {
    FC08_ENGINE_SCOPE
    const PrepareResult prepared = prepare(
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
    FC08_ENGINE_STAGED
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
    auto reserved_ledger = applied_operations.reserveContainerEntry(prepared.plan->operation);
    FC08_ENGINE_RESERVED
    if (internal::applyPreparedPlan(objects, *prepared.plan FC08_ATOMIC_ARGUMENT).status != internal::AtomicApplyStatus::kApplied) {
        return {ApplyDisposition::kRejected, {}, CommitBlockReason::kNone, std::nullopt};
    }
    if (!applied_operations.recordReservedOrApplied(prepared.plan->operation, std::move(reserved_ledger))) {
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
    FC08_ENGINE_DONE
    return {ApplyDisposition::kApplied, {}, CommitBlockReason::kNone, std::move(record)};
} catch (const std::bad_alloc&) {
    if (operation.kind() == OperationKind::kSetSectionContent || operation.kind() == OperationKind::kEditContainerTopology)
        return {ApplyDisposition::kRejected, {}, CommitBlockReason::kNone, std::nullopt};
    throw;
}

ApplyResult OperationEngine::apply(const Operation& operation, ApplySource source, ReferenceObjectStore& objects, ResourceManifest& manifest, AppliedOperationLedger& applied_operations, SemanticGenerationState& generation, CanonicalCommitClock& canonical_commit_clock) const try {
    FC08_ENGINE_SCOPE
    const PrepareResult prepared=prepare(operation, StatefulValidationContext{objects,applied_operations,&manifest});
    if(prepared.disposition==PrepareDisposition::kAlreadyApplied)return {ApplyDisposition::kAlreadyApplied,{},CommitBlockReason::kNone,std::nullopt};
    if(prepared.disposition==PrepareDisposition::kRejected||!prepared.plan)return {ApplyDisposition::kRejected,prepared.error,CommitBlockReason::kNone,std::nullopt};
    FC08_ENGINE_STAGED
    if(canonical_commit_clock.runtimeEpoch().value()==0)return {ApplyDisposition::kCommitBlocked,{},CommitBlockReason::kInvalidRuntimeEpoch,std::nullopt};
    const SemanticGeneration before=generation.current(); SemanticGeneration after; if(!generation.prepareSuccessor(after))return {ApplyDisposition::kRejected,{},CommitBlockReason::kNone,std::nullopt}; CommitOrdinal ordinal; if(!canonical_commit_clock.prepareSuccessor(ordinal))return {ApplyDisposition::kCommitBlocked,{},CommitBlockReason::kCommitLaneExhausted,std::nullopt}; auto changes=internal::prepareChangeSet(*prepared.plan); if(!changes)return {ApplyDisposition::kRejected,{},CommitBlockReason::kNone,std::nullopt};
    auto reserved_ledger = applied_operations.reserveContainerEntry(prepared.plan->operation);
    FC08_ENGINE_RESERVED
    if(internal::applyPreparedPlan(objects,manifest,*prepared.plan FC08_ATOMIC_ARGUMENT).status!=internal::AtomicApplyStatus::kApplied)return {ApplyDisposition::kRejected,{},CommitBlockReason::kNone,std::nullopt}; if(!applied_operations.recordReservedOrApplied(prepared.plan->operation, std::move(reserved_ledger)))std::terminate(); generation.commitSuccessor(after); canonical_commit_clock.commitSuccessor(ordinal); auto cs=internal::finalizeChangeSet(std::move(*changes),before,after); CanonicalCommitRecord record{prepared.plan->operation.id,source,before,after,CanonicalCommitStamp{canonical_commit_clock.runtimeEpoch(),ordinal},std::move(cs)}; FC08_ENGINE_DONE
    return {ApplyDisposition::kApplied,{},CommitBlockReason::kNone,std::move(record)};
} catch (const std::bad_alloc&) {
    if (operation.kind() == OperationKind::kSetSectionContent || operation.kind() == OperationKind::kEditContainerTopology)
        return {ApplyDisposition::kRejected, {}, CommitBlockReason::kNone, std::nullopt};
    throw;
}
ApplyResult OperationEngine::apply(const Operation& operation, ApplySource source, IndexedObjectStore& objects, ResourceManifest& manifest, AppliedOperationLedger& applied_operations, SemanticGenerationState& generation, CanonicalCommitClock& canonical_commit_clock) const try {
    FC08_ENGINE_SCOPE
    const PrepareResult prepared=prepare(operation, StatefulValidationContext{objects,applied_operations,&manifest});
    if(prepared.disposition==PrepareDisposition::kAlreadyApplied)return {ApplyDisposition::kAlreadyApplied,{},CommitBlockReason::kNone,std::nullopt};
    if(prepared.disposition==PrepareDisposition::kRejected||!prepared.plan)return {ApplyDisposition::kRejected,prepared.error,CommitBlockReason::kNone,std::nullopt};
    FC08_ENGINE_STAGED
    if(canonical_commit_clock.runtimeEpoch().value()==0)return {ApplyDisposition::kCommitBlocked,{},CommitBlockReason::kInvalidRuntimeEpoch,std::nullopt};
    const SemanticGeneration before=generation.current(); SemanticGeneration after; if(!generation.prepareSuccessor(after))return {ApplyDisposition::kRejected,{},CommitBlockReason::kNone,std::nullopt}; CommitOrdinal ordinal; if(!canonical_commit_clock.prepareSuccessor(ordinal))return {ApplyDisposition::kCommitBlocked,{},CommitBlockReason::kCommitLaneExhausted,std::nullopt}; auto changes=internal::prepareChangeSet(*prepared.plan); if(!changes)return {ApplyDisposition::kRejected,{},CommitBlockReason::kNone,std::nullopt};
    auto reserved_ledger = applied_operations.reserveContainerEntry(prepared.plan->operation);
    FC08_ENGINE_RESERVED
    if(internal::applyPreparedPlan(objects,manifest,*prepared.plan FC08_ATOMIC_ARGUMENT).status!=internal::AtomicApplyStatus::kApplied)return {ApplyDisposition::kRejected,{},CommitBlockReason::kNone,std::nullopt}; if(!applied_operations.recordReservedOrApplied(prepared.plan->operation, std::move(reserved_ledger)))std::terminate(); generation.commitSuccessor(after); canonical_commit_clock.commitSuccessor(ordinal); auto cs=internal::finalizeChangeSet(std::move(*changes),before,after); CanonicalCommitRecord record{prepared.plan->operation.id,source,before,after,CanonicalCommitStamp{canonical_commit_clock.runtimeEpoch(),ordinal},std::move(cs)}; FC08_ENGINE_DONE
    return {ApplyDisposition::kApplied,{},CommitBlockReason::kNone,std::move(record)};
} catch (const std::bad_alloc&) {
    if (operation.kind() == OperationKind::kSetSectionContent || operation.kind() == OperationKind::kEditContainerTopology)
        return {ApplyDisposition::kRejected, {}, CommitBlockReason::kNone, std::nullopt};
    throw;
}

} // namespace canvas::semantic
