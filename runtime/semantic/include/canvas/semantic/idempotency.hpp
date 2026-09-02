#pragma once

#include "canvas/semantic/operation.hpp"

#include <cstdint>

namespace canvas::semantic {

class AppliedOperationView;

enum class IdempotencyDisposition : std::uint8_t {
    kNew = 0,
    kAlreadyApplied,
    kCollision,
};

struct IdempotencyResult final {
    IdempotencyDisposition disposition = IdempotencyDisposition::kNew;
};

[[nodiscard]] IdempotencyResult classifyOperation(
    const Operation& operation,
    const AppliedOperationView& applied_operations);

} // namespace canvas::semantic
