#pragma once

#include "canvas/semantic/apply_plan.hpp"
#include "canvas/semantic/change_set.hpp"
#include "canvas/semantic/semantic_generation.hpp"

#include <cstdint>
#include <optional>

namespace canvas::semantic {

class AppliedOperationLedger;
class IndexedObjectStore;
class ReferenceObjectStore;

enum class ApplyDisposition : std::uint8_t {
    kApplied = 0,
    kAlreadyApplied,
    kRejected,
};

struct ApplyResult final {
    ApplyDisposition disposition = ApplyDisposition::kRejected;
    StatefulResult error{};
    std::optional<ChangeSet> change_set;
};

class OperationEngine final {
  public:
    [[nodiscard]] PrepareResult prepare(
        const Operation& operation,
        const StatefulValidationContext& context) const;

    [[nodiscard]] ApplyResult apply(
        const Operation& operation,
        ReferenceObjectStore& objects,
        AppliedOperationLedger& applied_operations,
        SemanticGenerationState& generation) const;

    [[nodiscard]] ApplyResult apply(
        const Operation& operation,
        IndexedObjectStore& objects,
        AppliedOperationLedger& applied_operations,
        SemanticGenerationState& generation) const;
};

} // namespace canvas::semantic
