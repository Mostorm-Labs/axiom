#pragma once

#include "canvas/semantic/staged_object_view.hpp"
#include "canvas/semantic/stateful_validation.hpp"

#include <span>

namespace canvas::semantic {

[[nodiscard]] StatefulResult validateStagedHierarchyCapabilities(
    const StagedObjectView& staged,
    std::span<const ObjectId> affected_child_ids);

} // namespace canvas::semantic
