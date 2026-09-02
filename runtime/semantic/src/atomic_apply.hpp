#pragma once

#include "canvas/semantic/apply_plan.hpp"
#include "canvas/semantic/indexed_object_store.hpp"
#include "canvas/semantic/reference_object_store.hpp"

#include <cstdint>

namespace canvas::semantic::internal {

enum class AtomicApplyStatus : std::uint8_t {
    kApplied = 0,
    kPreconditionFailed,
};

struct AtomicApplyResult final {
    AtomicApplyStatus status = AtomicApplyStatus::kPreconditionFailed;
};

[[nodiscard]] AtomicApplyResult applyPreparedPlan(
    ReferenceObjectStore& store,
    const PreparedApplyPlan& plan);

[[nodiscard]] AtomicApplyResult applyPreparedPlan(
    IndexedObjectStore& store,
    const PreparedApplyPlan& plan);

} // namespace canvas::semantic::internal
