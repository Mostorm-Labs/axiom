#pragma once

#include "canvas/semantic/stateful_validation.hpp"

#include <map>
#include <optional>

namespace canvas::semantic {

class OperationEngine;

class AppliedOperationLedger final : public AppliedOperationView {
  public:
    [[nodiscard]] std::optional<AppliedOperationEntry> find(
        const OperationId& id) const override;

  private:
    friend class OperationEngine;

    [[nodiscard]] bool recordApplied(const Operation& canonical_operation);

    std::map<OperationId, AppliedOperationEntry> entries_;
};

} // namespace canvas::semantic
