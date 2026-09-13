#pragma once

#include "canvas/scene/scene_types.hpp"

#include <cstdint>
#include <span>
#include <vector>

namespace canvas {

using RecordHandle = std::uint64_t;

struct SceneOrderChange final {
    ObjectId objectId;
    SceneOrderKey before;
    SceneOrderKey after;
};

struct SceneBoundsChange final {
    ObjectId objectId;
    WorldRect before;
    WorldRect after;
};

// Runtime execution-layer delta. It is deliberately independent of semantic
// Operation/ChangeSet and is materialized from a compiled scene delta.
struct SceneDelta final {
    SceneRevision generationFrom;
    SceneRevision generationTo;
    std::vector<SceneRecord> addedRecords;
    std::vector<SceneMutation> updatedRecords;
    std::vector<RecordHandle> removedHandles;
    std::vector<SceneOrderChange> orderChanges;
    std::vector<SceneBoundsChange> boundsChanges;
    std::vector<SceneMutation> mutations;
};

[[nodiscard]] SceneDelta makeSceneDelta(const CompiledSceneDelta& delta);

} // namespace canvas
