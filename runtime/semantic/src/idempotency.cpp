#include "canvas/semantic/idempotency.hpp"

#include "canvas/semantic/operation_fingerprint.hpp"
#include "canvas/semantic/stateful_validation.hpp"

namespace canvas::semantic {

IdempotencyResult classifyOperation(
    const Operation& operation,
    const AppliedOperationView& applied_operations) {
    const auto entry = applied_operations.find(operation.id);
    if (!entry.has_value()) {
        return IdempotencyResult{IdempotencyDisposition::kNew};
    }
    if (canonicalPayloadEqual(entry->canonical_operation, operation)) {
        return IdempotencyResult{IdempotencyDisposition::kAlreadyApplied};
    }
    return IdempotencyResult{IdempotencyDisposition::kCollision};
}

} // namespace canvas::semantic
