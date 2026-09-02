#include "canvas/semantic/operation_engine.hpp"

#include "canvas/semantic/applied_operation_ledger.hpp"

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
    ReferenceObjectStore& objects,
    AppliedOperationLedger& applied_operations,
    SemanticGenerationState& generation) const {
    const PrepareResult prepared = prepare(operation, StatefulValidationContext{objects, applied_operations});
    if (prepared.disposition == PrepareDisposition::kAlreadyApplied) {
        return {ApplyDisposition::kAlreadyApplied, {}, std::nullopt};
    }
    if (prepared.disposition == PrepareDisposition::kRejected) {
        return {ApplyDisposition::kRejected, prepared.error, std::nullopt};
    }
    if (!prepared.plan.has_value()) {
        std::terminate();
    }

    const SemanticGeneration before = generation.current();
    SemanticGeneration after;
    if (!generation.prepareSuccessor(after)) {
        return {ApplyDisposition::kRejected, {}, std::nullopt};
    }
    const auto prepared_changes = internal::prepareChangeSet(*prepared.plan);
    if (!prepared_changes.has_value()) {
        return {ApplyDisposition::kRejected, {}, std::nullopt};
    }

    if (internal::applyPreparedPlan(objects, *prepared.plan).status != internal::AtomicApplyStatus::kApplied) {
        return {ApplyDisposition::kRejected, {}, std::nullopt};
    }
    if (!applied_operations.recordApplied(prepared.plan->operation)) {
        std::terminate();
    }
    generation.commitSuccessor(after);
    return {
        ApplyDisposition::kApplied,
        {},
        internal::finalizeChangeSet(std::move(*prepared_changes), before, after)};
}

ApplyResult OperationEngine::apply(
    const Operation& operation,
    IndexedObjectStore& objects,
    AppliedOperationLedger& applied_operations,
    SemanticGenerationState& generation) const {
    const PrepareResult prepared = prepare(operation, StatefulValidationContext{objects, applied_operations});
    if (prepared.disposition == PrepareDisposition::kAlreadyApplied) {
        return {ApplyDisposition::kAlreadyApplied, {}, std::nullopt};
    }
    if (prepared.disposition == PrepareDisposition::kRejected) {
        return {ApplyDisposition::kRejected, prepared.error, std::nullopt};
    }
    if (!prepared.plan.has_value()) {
        std::terminate();
    }

    const SemanticGeneration before = generation.current();
    SemanticGeneration after;
    if (!generation.prepareSuccessor(after)) {
        return {ApplyDisposition::kRejected, {}, std::nullopt};
    }
    const auto prepared_changes = internal::prepareChangeSet(*prepared.plan);
    if (!prepared_changes.has_value()) {
        return {ApplyDisposition::kRejected, {}, std::nullopt};
    }

    if (internal::applyPreparedPlan(objects, *prepared.plan).status != internal::AtomicApplyStatus::kApplied) {
        return {ApplyDisposition::kRejected, {}, std::nullopt};
    }
    if (!applied_operations.recordApplied(prepared.plan->operation)) {
        std::terminate();
    }
    generation.commitSuccessor(after);
    return {
        ApplyDisposition::kApplied,
        {},
        internal::finalizeChangeSet(std::move(*prepared_changes), before, after)};
}

} // namespace canvas::semantic
