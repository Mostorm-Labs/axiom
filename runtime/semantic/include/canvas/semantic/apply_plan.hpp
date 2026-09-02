#pragma once

#include "canvas/semantic/delete_closure.hpp"
#include "canvas/semantic/operation.hpp"
#include "canvas/semantic/restore_planner.hpp"
#include "canvas/semantic/operation_specific_validation.hpp"

#include <cstdint>
#include <optional>
#include <vector>

namespace canvas::semantic {

enum class PrepareDisposition : std::uint8_t {
    kPrepared = 0,
    kAlreadyApplied,
    kRejected,
};

struct PreparedApplyPlan final {
    Operation operation{};
    std::vector<ObjectRecord> creates;
    std::vector<ObjectRecord> replacements;
    std::vector<ObjectId> deletes;
    std::optional<DeleteClosure> delete_closure;
};

struct PrepareResult final {
    PrepareDisposition disposition = PrepareDisposition::kRejected;
    StatefulResult error{};
    std::optional<PreparedApplyPlan> plan;
};

[[nodiscard]] PrepareResult prepareApplyPlan(
    const Operation& operation,
    const StatefulValidationContext& context);

} // namespace canvas::semantic
