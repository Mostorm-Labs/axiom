#pragma once

#include "canvas/semantic/apply_plan.hpp"

#include <cstdint>

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
};

class OperationEngine final {
  public:
    [[nodiscard]] PrepareResult prepare(
        const Operation& operation,
        const StatefulValidationContext& context) const;

    [[nodiscard]] ApplyResult apply(
        const Operation& operation,
        ReferenceObjectStore& objects,
        AppliedOperationLedger& applied_operations) const;

    [[nodiscard]] ApplyResult apply(
        const Operation& operation,
        IndexedObjectStore& objects,
        AppliedOperationLedger& applied_operations) const;
};

} // namespace canvas::semantic
