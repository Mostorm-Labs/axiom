#pragma once

#include "canvas/semantic/apply_plan.hpp"
#include "canvas/semantic/change_set.hpp"

#include <optional>
#include <vector>

namespace canvas::semantic::internal {

// Prepared semantic impact. This is an internal, pre-mutation description;
// it is finalized into an immutable ChangeSet only after atomic apply and the
// A1 ledger write have succeeded.
struct PreparedChangeSet final {
    std::vector<ObjectSemanticChange> changes;
};

[[nodiscard]] std::optional<PreparedChangeSet> prepareChangeSet(
    const PreparedApplyPlan& plan);

[[nodiscard]] ChangeSet finalizeChangeSet(
    PreparedChangeSet prepared,
    SemanticGeneration before,
    SemanticGeneration after);

} // namespace canvas::semantic::internal
