#pragma once

#include "canvas/semantic/object_record.hpp"
#include "canvas/semantic/staged_object_view.hpp"
#include "canvas/semantic/stateful_validation.hpp"

#include <span>
#include <vector>

namespace canvas::semantic {

struct HierarchyEdit final {
    ObjectId object_id;
    Placement placement;
};

[[nodiscard]] StatefulResult validateStagedHierarchy(
    const StagedObjectView& staged,
    std::span<const HierarchyEdit> edits);

[[nodiscard]] std::vector<ObjectId> resolveDescendants(
    const StagedObjectView& staged,
    ObjectId root_id);

} // namespace canvas::semantic
