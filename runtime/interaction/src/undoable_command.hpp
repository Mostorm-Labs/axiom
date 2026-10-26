#pragma once
#include "canvas/interaction/editor_history.hpp"
#include "canvas/semantic/operation_engine.hpp"
#include "canvas/semantic/staged_object_view.hpp"

namespace canvas::interaction {
[[nodiscard]] bool supportsUndoableCommand(semantic::OperationKind) noexcept;
// Reused FC08 commands are selected by the existing owner, not by the host.
[[nodiscard]] bool isContainerCommand(const semantic::Operation&, const semantic::ObjectStore&) noexcept;
struct PreparedUndoableCommand final {
    semantic::Operation forward, inverse;
    std::vector<HistoryExpectation> before, after;
    std::size_t forwardBytes = 0, inverseBytes = 0;
};
[[nodiscard]] std::optional<PreparedUndoableCommand> prepareUndoableCommand(
    const semantic::PreparedApplyPlan&, const semantic::StatefulValidationContext&,
    const semantic::StagedObjectView&);
[[nodiscard]] bool historyFootprintMatches(const semantic::PreparedApplyPlan&,
    std::span<const HistoryExpectation>);
}
