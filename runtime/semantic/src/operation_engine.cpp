#include "canvas/semantic/operation_engine.hpp"

#include "canvas/semantic/applied_operation_ledger.hpp"

#include "atomic_apply.hpp"

#include <exception>

namespace canvas::semantic {

PrepareResult OperationEngine::prepare(const Operation& operation, const StatefulValidationContext& context) const {
    return prepareApplyPlan(operation, context);
}

ApplyResult OperationEngine::apply(
    const Operation& operation,
    ReferenceObjectStore& objects,
    AppliedOperationLedger& applied_operations) const {
    const PrepareResult prepared = prepare(operation, StatefulValidationContext{objects, applied_operations});
    if (prepared.disposition == PrepareDisposition::kAlreadyApplied) {
        return {ApplyDisposition::kAlreadyApplied, {}};
    }
    if (prepared.disposition == PrepareDisposition::kRejected) {
        return {ApplyDisposition::kRejected, prepared.error};
    }
    if (!prepared.plan.has_value()) {
        std::terminate();
    }

    if (internal::applyPreparedPlan(objects, *prepared.plan).status != internal::AtomicApplyStatus::kApplied) {
        return {ApplyDisposition::kRejected, {}};
    }
    if (!applied_operations.recordApplied(prepared.plan->operation)) {
        std::terminate();
    }
    return {ApplyDisposition::kApplied, {}};
}

ApplyResult OperationEngine::apply(
    const Operation& operation,
    IndexedObjectStore& objects,
    AppliedOperationLedger& applied_operations) const {
    const PrepareResult prepared = prepare(operation, StatefulValidationContext{objects, applied_operations});
    if (prepared.disposition == PrepareDisposition::kAlreadyApplied) {
        return {ApplyDisposition::kAlreadyApplied, {}};
    }
    if (prepared.disposition == PrepareDisposition::kRejected) {
        return {ApplyDisposition::kRejected, prepared.error};
    }
    if (!prepared.plan.has_value()) {
        std::terminate();
    }

    if (internal::applyPreparedPlan(objects, *prepared.plan).status != internal::AtomicApplyStatus::kApplied) {
        return {ApplyDisposition::kRejected, {}};
    }
    if (!applied_operations.recordApplied(prepared.plan->operation)) {
        std::terminate();
    }
    return {ApplyDisposition::kApplied, {}};
}

} // namespace canvas::semantic
