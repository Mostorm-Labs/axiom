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

std::optional<AppliedOperationLedger::ReservedEntry> AppliedOperationLedger::reserveContainerEntry(const Operation& operation) {
    if (operation.kind() != OperationKind::kSetSectionContent && operation.kind() != OperationKind::kEditContainerTopology)
        return std::nullopt;
    std::map<OperationId, AppliedOperationEntry> reservation;
    reservation.emplace(operation.id, AppliedOperationEntry{operation, std::nullopt});
    return reservation.extract(operation.id);
}

bool AppliedOperationLedger::recordReservedOrApplied(const Operation& operation, std::optional<ReservedEntry> reserved) {
    if (!reserved) return recordApplied(operation);
    return entries_.insert(std::move(*reserved)).inserted;
}

} // namespace canvas::semantic
