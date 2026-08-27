#pragma once

#include "canvas/semantic/codec.hpp"
#include "canvas/semantic/operation.hpp"

#include <cstdint>

namespace canvas::semantic {

enum class ValidationIssue : std::uint8_t {
    kNone = 0,
    kInvalidId,
    kUnsupportedVersion,
    kInvalidCollection,
    kInvalidObjectKind,
    kInvalidPropertyPatch,
    kInvalidLeaf,
    kGeometryLimitExceeded,
    kIntegerOverflow,
};

// This is a local implementation diagnostic only.  GT-G1-04-C owns any
// future stable protocol stage/path/category mapping.
struct ValidationResult final {
    ValidationIssue issue = ValidationIssue::kNone;
    [[nodiscard]] bool ok() const noexcept { return issue == ValidationIssue::kNone; }
};

[[nodiscard]] ValidationResult validateEnvelope(
    const Operation& operation,
    const OperationFieldPresence& presence) noexcept;

[[nodiscard]] ValidationResult validatePayloadStructure(const Operation& operation) noexcept;

} // namespace canvas::semantic
