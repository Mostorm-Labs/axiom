#include "canvas/semantic/operation.hpp"

namespace canvas::semantic {

bool isKnownObjectKind(ObjectKind kind) noexcept {
    const auto value = static_cast<std::uint8_t>(kind);
    return value >= 1U && value <= 9U;
}

bool isKnownOperationKind(OperationKind kind) noexcept {
    const auto value = static_cast<std::uint8_t>(kind);
    return value >= 1U && value <= 15U;
}

} // namespace canvas::semantic
