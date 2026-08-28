#pragma once

#include "canvas/semantic/operation.hpp"

#include <cstdint>
#include <vector>

namespace canvas::semantic {

using OperationFingerprint = std::vector<std::uint8_t>;

// Equality over an already-normalized typed Operation. OperationId and any
// source/transport metadata are intentionally outside this semantic payload.
[[nodiscard]] bool canonicalPayloadEqual(const Operation& lhs, const Operation& rhs) noexcept;

} // namespace canvas::semantic
