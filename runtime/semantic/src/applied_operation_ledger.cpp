#include "canvas/semantic/applied_operation_ledger.hpp"

namespace canvas::semantic {

std::optional<AppliedOperationEntry> AppliedOperationLedger::find(const OperationId& id) const {
    const auto it = entries_.find(id);
    if (it == entries_.end()) {
        return std::nullopt;
    }
    return it->second;
}

bool AppliedOperationLedger::recordApplied(const Operation& canonical_operation) {
    return entries_.emplace(
                       canonical_operation.id,
                       AppliedOperationEntry{canonical_operation, std::nullopt})
        .second;
}

} // namespace canvas::semantic
