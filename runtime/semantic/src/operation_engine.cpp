#include "canvas/semantic/operation_engine.hpp"

#include "canvas/semantic/applied_operation_ledger.hpp"
#include "canvas/semantic/indexed_object_store.hpp"
#include "canvas/semantic/reference_object_store.hpp"
#include "canvas/semantic/normalizer.hpp"
#include "canvas/semantic/validator.hpp"

#include "atomic_apply.hpp"
#include "object_store_mutator.hpp"
#include "canvas/semantic/staged_object_view.hpp"
#include "change_set_builder.hpp"

#include <exception>
#include <new>
#include <limits>
#include <set>
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
class PreparedOperationIntent::State final {
  public:
    State(IndexedObjectStore& store, AppliedOperationLedger& ledger)
        : staged(store), objects(store), applied(ledger) {}
    StagedObjectView staged;
    IndexedObjectStore& objects;
    AppliedOperationLedger& applied;
    SemanticGeneration basis;
    CommitOrdinal ordinal;
    RuntimeEpoch epoch;
    std::vector<PreparedApplyPlan> plans;
    std::vector<std::unique_ptr<internal::PreparedIndexedContainerApply>> storage;
    std::vector<std::map<OperationId, AppliedOperationEntry>::node_type> ledger;
    std::vector<CanonicalCommitRecord> records;
    bool consumed = false;
};

PreparedOperationIntent::PreparedOperationIntent(std::unique_ptr<State> state)
    : state_(std::move(state)) {}
PreparedOperationIntent::~PreparedOperationIntent() = default;
const StagedObjectView& PreparedOperationIntent::postState() const noexcept { return state_->staged; }
const std::vector<PreparedApplyPlan>& PreparedOperationIntent::plans() const noexcept { return state_->plans; }
const std::vector<CanonicalCommitRecord>& PreparedOperationIntent::records() const noexcept { return state_->records; }
CanonicalCommitRecord PreparedOperationIntent::takeLastRecord() noexcept {
    return std::move(state_->records.back());
}

std::unique_ptr<PreparedOperationIntent> OperationEngine::prepareIntent(
    std::span<const Operation> operations, ApplySource source, IndexedObjectStore& objects,
    ResourceManifest& manifest, AppliedOperationLedger& applied,
    const SemanticGenerationState& generation, const CanonicalCommitClock& clock) const try {
#if defined(AXIOM_FC08_QUALIFICATION_HOOKS)
    // The new path must never bypass the existing qualification checkpoints.
    // Its hook-aware implementation is not yet available.
    (void)operations; (void)source; (void)objects; (void)manifest; (void)applied;
    (void)generation; (void)clock;
    return {};
#else
    if (operations.empty() || clock.runtimeEpoch().value() == 0 ||
        operations.size() > std::numeric_limits<std::uint64_t>::max() - generation.current().value() ||
        operations.size() > std::numeric_limits<std::uint64_t>::max() - clock.lastCommittedOrdinal().value()) return {};
    auto state = std::make_unique<PreparedOperationIntent::State>(objects, applied);
    state->basis = generation.current();
    state->ordinal = clock.lastCommittedOrdinal();
    state->epoch = clock.runtimeEpoch();
    state->plans.reserve(operations.size());
    state->storage.reserve(operations.size());
    state->ledger.reserve(operations.size());
    state->records.reserve(operations.size());
    std::set<OperationId> ids;
    for (const auto& operation : operations) {
        // The currently proved reservation path covers these existing typed
        // mutations; other kinds fail closed instead of committing a prefix.
        if ((operation.kind() != OperationKind::kSetTransforms &&
             operation.kind() != OperationKind::kPatchProperties) ||
            !ids.insert(operation.id).second) return {};
        auto prepared = prepare(operation, {state->staged, applied, &manifest});
        if (prepared.disposition != PrepareDisposition::kPrepared || !prepared.plan ||
            prepared.plan->manifest || !prepared.plan->creates.empty() || !prepared.plan->deletes.empty()) return {};
        auto storage = internal::ObjectStoreMutator::prepareContainerPlan(objects, *prepared.plan, true);
        auto changes = internal::prepareChangeSet(*prepared.plan);
        auto reservation = applied.reserveContainerEntry(prepared.plan->operation, true);
        if (!storage || !changes || !reservation) return {};
        for (const auto& item : prepared.plan->replacements)
            if (!state->staged.stageReplace(item)) return {};
        const auto i = state->plans.size();
        const auto before = SemanticGeneration(state->basis.value() + i);
        const auto after = SemanticGeneration(before.value() + 1U);
        const auto ordinal = CommitOrdinal(state->ordinal.value() + i + 1U);
        state->records.push_back(CanonicalCommitRecord{
            prepared.plan->operation.id, source, before, after,
            CanonicalCommitStamp{state->epoch, ordinal},
            internal::finalizeChangeSet(std::move(*changes), before, after)});
        state->plans.push_back(std::move(*prepared.plan));
        state->storage.push_back(std::move(storage));
        state->ledger.push_back(std::move(*reservation));
    }
    return std::unique_ptr<PreparedOperationIntent>(new PreparedOperationIntent(std::move(state)));
#endif
} catch (const std::bad_alloc&) { return {}; }

bool OperationEngine::commitIntent(PreparedOperationIntent& intent, SemanticGenerationState& generation,
                                   CanonicalCommitClock& clock) const noexcept {
    auto& s = *intent.state_;
    if (s.consumed || generation.current() != s.basis || clock.runtimeEpoch() != s.epoch ||
        clock.lastCommittedOrdinal() != s.ordinal) return false;
    s.consumed = true;
    for (std::size_t i = 0; i < s.plans.size(); ++i) {
        s.storage[i]->publish();
        if (!s.applied.recordReservedOrApplied(s.plans[i].operation, std::move(s.ledger[i])))
            std::terminate();
        generation.commitSuccessor(s.records[i].after_generation);
        clock.commitSuccessor(s.records[i].commit_stamp.ordinal);
    }
    return true;
}
namespace {
bool admitPlan(OperationAdmission* admission, const PreparedApplyPlan& plan,
               const StatefulValidationContext& context,
               const SemanticGenerationState& generation, const CanonicalCommitClock& clock) {
    if (!admission) return true;
    const auto kind = plan.operation.kind();
    if (plan.manifest || (kind != OperationKind::kSetSectionContent &&
        kind != OperationKind::kEditContainerTopology && kind != OperationKind::kInsertObjects &&
        kind != OperationKind::kRestoreObjects && kind != OperationKind::kDeleteObjects &&
        kind != OperationKind::kSetTransforms && kind != OperationKind::kPatchProperties)) return false;
    const auto basisGeneration = generation.current();
    const auto basisOrdinal = clock.lastCommittedOrdinal();
    StagedObjectView after(context.objects);
    for (const auto& r : plan.creates) if (!after.stageCreate(r)) return false;
    for (const auto& r : plan.replacements) if (!after.stageReplace(r)) return false;
    for (const auto& id : plan.deletes) if (!after.stageDelete(id)) return false;
    return admission->admit(plan, context, after) &&
        generation.current() == basisGeneration && clock.lastCommittedOrdinal() == basisOrdinal;
}
}
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
    CanonicalCommitClock& canonical_commit_clock, OperationAdmission* admission) const try {
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

    if (!admitPlan(admission, *prepared.plan, {objects, applied_operations}, generation, canonical_commit_clock))
        return {ApplyDisposition::kRejected, {}, CommitBlockReason::kNone, std::nullopt};
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

    auto reserved_ledger = applied_operations.reserveContainerEntry(prepared.plan->operation, admission != nullptr);
    FC08_ENGINE_RESERVED
    if (internal::applyPreparedPlan(objects, *prepared.plan, admission != nullptr FC08_ATOMIC_ARGUMENT).status != internal::AtomicApplyStatus::kApplied) {
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
    if (admission || operation.kind() == OperationKind::kSetSectionContent || operation.kind() == OperationKind::kEditContainerTopology)
        return {ApplyDisposition::kRejected, {}, CommitBlockReason::kNone, std::nullopt};
    throw;
}

ApplyResult OperationEngine::apply(
    const Operation& operation,
    ApplySource source,
    IndexedObjectStore& objects,
    AppliedOperationLedger& applied_operations,
    SemanticGenerationState& generation,
    CanonicalCommitClock& canonical_commit_clock, OperationAdmission* admission) const try {
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
    if (!admitPlan(admission, *prepared.plan, {objects, applied_operations}, generation, canonical_commit_clock))
        return {ApplyDisposition::kRejected, {}, CommitBlockReason::kNone, std::nullopt};
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
    auto reserved_ledger = applied_operations.reserveContainerEntry(prepared.plan->operation, admission != nullptr);
    FC08_ENGINE_RESERVED
    if (internal::applyPreparedPlan(objects, *prepared.plan, admission != nullptr FC08_ATOMIC_ARGUMENT).status != internal::AtomicApplyStatus::kApplied) {
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
    if (admission || operation.kind() == OperationKind::kSetSectionContent || operation.kind() == OperationKind::kEditContainerTopology)
        return {ApplyDisposition::kRejected, {}, CommitBlockReason::kNone, std::nullopt};
    throw;
}

ApplyResult OperationEngine::apply(const Operation& operation, ApplySource source, ReferenceObjectStore& objects, ResourceManifest& manifest, AppliedOperationLedger& applied_operations, SemanticGenerationState& generation, CanonicalCommitClock& canonical_commit_clock, OperationAdmission* admission) const try {
    FC08_ENGINE_SCOPE
    const PrepareResult prepared=prepare(operation, StatefulValidationContext{objects,applied_operations,&manifest});
    if(prepared.disposition==PrepareDisposition::kAlreadyApplied)return {ApplyDisposition::kAlreadyApplied,{},CommitBlockReason::kNone,std::nullopt};
    if(prepared.disposition==PrepareDisposition::kRejected||!prepared.plan)return {ApplyDisposition::kRejected,prepared.error,CommitBlockReason::kNone,std::nullopt};
    if (!admitPlan(admission, *prepared.plan, {objects, applied_operations, &manifest}, generation, canonical_commit_clock))
        return {ApplyDisposition::kRejected, {}, CommitBlockReason::kNone, std::nullopt};
    FC08_ENGINE_STAGED
    if(canonical_commit_clock.runtimeEpoch().value()==0)return {ApplyDisposition::kCommitBlocked,{},CommitBlockReason::kInvalidRuntimeEpoch,std::nullopt};
    const SemanticGeneration before=generation.current(); SemanticGeneration after; if(!generation.prepareSuccessor(after))return {ApplyDisposition::kRejected,{},CommitBlockReason::kNone,std::nullopt}; CommitOrdinal ordinal; if(!canonical_commit_clock.prepareSuccessor(ordinal))return {ApplyDisposition::kCommitBlocked,{},CommitBlockReason::kCommitLaneExhausted,std::nullopt}; auto changes=internal::prepareChangeSet(*prepared.plan); if(!changes)return {ApplyDisposition::kRejected,{},CommitBlockReason::kNone,std::nullopt};
    auto reserved_ledger = applied_operations.reserveContainerEntry(prepared.plan->operation, admission != nullptr);
    FC08_ENGINE_RESERVED
    if(internal::applyPreparedPlan(objects,manifest,*prepared.plan, admission != nullptr FC08_ATOMIC_ARGUMENT).status!=internal::AtomicApplyStatus::kApplied)return {ApplyDisposition::kRejected,{},CommitBlockReason::kNone,std::nullopt}; if(!applied_operations.recordReservedOrApplied(prepared.plan->operation, std::move(reserved_ledger)))std::terminate(); generation.commitSuccessor(after); canonical_commit_clock.commitSuccessor(ordinal); auto cs=internal::finalizeChangeSet(std::move(*changes),before,after); CanonicalCommitRecord record{prepared.plan->operation.id,source,before,after,CanonicalCommitStamp{canonical_commit_clock.runtimeEpoch(),ordinal},std::move(cs)}; FC08_ENGINE_DONE
    return {ApplyDisposition::kApplied,{},CommitBlockReason::kNone,std::move(record)};
} catch (const std::bad_alloc&) {
    if (admission || operation.kind() == OperationKind::kSetSectionContent || operation.kind() == OperationKind::kEditContainerTopology)
        return {ApplyDisposition::kRejected, {}, CommitBlockReason::kNone, std::nullopt};
    throw;
}
ApplyResult OperationEngine::apply(const Operation& operation, ApplySource source, IndexedObjectStore& objects, ResourceManifest& manifest, AppliedOperationLedger& applied_operations, SemanticGenerationState& generation, CanonicalCommitClock& canonical_commit_clock, OperationAdmission* admission) const try {
    FC08_ENGINE_SCOPE
    const PrepareResult prepared=prepare(operation, StatefulValidationContext{objects,applied_operations,&manifest});
    if(prepared.disposition==PrepareDisposition::kAlreadyApplied)return {ApplyDisposition::kAlreadyApplied,{},CommitBlockReason::kNone,std::nullopt};
    if(prepared.disposition==PrepareDisposition::kRejected||!prepared.plan)return {ApplyDisposition::kRejected,prepared.error,CommitBlockReason::kNone,std::nullopt};
    if (!admitPlan(admission, *prepared.plan, {objects, applied_operations, &manifest}, generation, canonical_commit_clock))
        return {ApplyDisposition::kRejected, {}, CommitBlockReason::kNone, std::nullopt};
    FC08_ENGINE_STAGED
    if(canonical_commit_clock.runtimeEpoch().value()==0)return {ApplyDisposition::kCommitBlocked,{},CommitBlockReason::kInvalidRuntimeEpoch,std::nullopt};
    const SemanticGeneration before=generation.current(); SemanticGeneration after; if(!generation.prepareSuccessor(after))return {ApplyDisposition::kRejected,{},CommitBlockReason::kNone,std::nullopt}; CommitOrdinal ordinal; if(!canonical_commit_clock.prepareSuccessor(ordinal))return {ApplyDisposition::kCommitBlocked,{},CommitBlockReason::kCommitLaneExhausted,std::nullopt}; auto changes=internal::prepareChangeSet(*prepared.plan); if(!changes)return {ApplyDisposition::kRejected,{},CommitBlockReason::kNone,std::nullopt};
    auto reserved_ledger = applied_operations.reserveContainerEntry(prepared.plan->operation, admission != nullptr);
    FC08_ENGINE_RESERVED
    if(internal::applyPreparedPlan(objects,manifest,*prepared.plan, admission != nullptr FC08_ATOMIC_ARGUMENT).status!=internal::AtomicApplyStatus::kApplied)return {ApplyDisposition::kRejected,{},CommitBlockReason::kNone,std::nullopt}; if(!applied_operations.recordReservedOrApplied(prepared.plan->operation, std::move(reserved_ledger)))std::terminate(); generation.commitSuccessor(after); canonical_commit_clock.commitSuccessor(ordinal); auto cs=internal::finalizeChangeSet(std::move(*changes),before,after); CanonicalCommitRecord record{prepared.plan->operation.id,source,before,after,CanonicalCommitStamp{canonical_commit_clock.runtimeEpoch(),ordinal},std::move(cs)}; FC08_ENGINE_DONE
    return {ApplyDisposition::kApplied,{},CommitBlockReason::kNone,std::move(record)};
} catch (const std::bad_alloc&) {
    if (admission || operation.kind() == OperationKind::kSetSectionContent || operation.kind() == OperationKind::kEditContainerTopology)
        return {ApplyDisposition::kRejected, {}, CommitBlockReason::kNone, std::nullopt};
    throw;
}

} // namespace canvas::semantic
