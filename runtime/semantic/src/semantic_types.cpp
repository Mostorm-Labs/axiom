#include "canvas/semantic/operation.hpp"
namespace canvas::semantic {

OperationKind operationKind(const OperationPayload& payload) noexcept {
    return static_cast<OperationKind>(payload.index() + 1U);
}

OperationKind Operation::kind() const noexcept {
    return operationKind(payload);
}

bool isKnownObjectKind(ObjectKind kind) noexcept {
    const auto value = static_cast<std::uint8_t>(kind);
    return value >= 1U && value <= 9U;
}

bool isKnownOperationKind(OperationKind kind) noexcept {
    const auto value = static_cast<std::uint8_t>(kind);
    return value >= 1U && value <= 15U;
}

} // namespace canvas::semantic
