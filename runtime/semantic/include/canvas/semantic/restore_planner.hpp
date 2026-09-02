#pragma once

#include "canvas/semantic/object_store.hpp"
#include "canvas/semantic/operation_payload.hpp"
#include "canvas/semantic/stateful_validation.hpp"

#include <vector>

namespace canvas::semantic {

struct RestorePlanInputs final {
    std::vector<ObjectRecord> creates;
};

[[nodiscard]] StatefulResult validateRestoreObjects(
    const RestoreObjectsOp& restore,
    const ObjectStore& apply_base,
    RestorePlanInputs* out);

} // namespace canvas::semantic
