#pragma once

#include "canvas/semantic/delete_closure.hpp"

#include <cstddef>
#include <span>
#include <vector>

namespace canvas::semantic::internal {

struct DeleteClosureWave final {
    std::vector<ObjectId> hierarchy_additions;
    std::vector<ObjectId> connector_additions;
};

struct DeleteClosureTrace final {
    std::size_t fixed_point_waves = 0;
    std::size_t reverse_relation_lookups = 0;
    std::vector<DeleteClosureWave> waves;
};

StatefulResult resolveDeleteClosureWithTrace(
    const StagedObjectView& staged,
    std::span<const ObjectId> requested_ids,
    DeleteClosure* out,
    DeleteClosureTrace* trace);

} // namespace canvas::semantic::internal
