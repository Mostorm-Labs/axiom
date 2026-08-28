#pragma once

#include "canvas/semantic/staged_object_view.hpp"
#include "canvas/semantic/stateful_validation.hpp"

#include <span>
#include <vector>

namespace canvas::semantic {

struct DeleteClosure final {
    std::vector<ObjectId> requested_delete_ids;
    std::vector<ObjectId> resolved_hierarchy_closure;
    std::vector<ObjectId> resolved_connector_cascade_closure;
    std::vector<ObjectId> final_delete_set;
};

StatefulResult resolveDeleteClosure(
    const StagedObjectView& staged,
    std::span<const ObjectId> requested_ids,
    DeleteClosure* out);

} // namespace canvas::semantic
