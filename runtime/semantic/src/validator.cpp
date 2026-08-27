#include "canvas/semantic/validator.hpp"

namespace canvas::semantic {

ValidationResult validateEnvelope(
    const Operation& operation,
    const OperationFieldPresence& presence) noexcept {
    if (operation.id.isZero() || operation.document_id.isZero()) {
        return {ValidationIssue::kInvalidId};
    }
    if (!presence.schema_version || !presence.payload_version ||
        operation.schema_version != 1U || operation.payload_version != 1U) {
        return {ValidationIssue::kUnsupportedVersion};
    }
    return {};
}

ValidationResult validatePayloadStructure(const Operation&) noexcept {
    return {};
}

} // namespace canvas::semantic
