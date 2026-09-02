#include "canvas/semantic/operation_engine.hpp"

namespace canvas::semantic {
PrepareResult OperationEngine::prepare(const Operation& operation, const StatefulValidationContext& context) const {
    return prepareApplyPlan(operation, context);
}
} // namespace canvas::semantic
