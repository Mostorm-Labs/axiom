#pragma once

#include "canvas/semantic/apply_plan.hpp"

namespace canvas::semantic {

class OperationEngine final {
  public:
    [[nodiscard]] PrepareResult prepare(
        const Operation& operation,
        const StatefulValidationContext& context) const;
};

} // namespace canvas::semantic
