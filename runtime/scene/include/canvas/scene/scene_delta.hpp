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

struct SlotWrite final {
    RecordHandle handle;
    std::optional<SceneRecord> before;
    std::optional<SceneRecord> after;
};

struct ObjectIndexUpdate final {
    ObjectId objectId;
    RecordHandle handle;
    bool remove = false;
};

struct OrderIndexUpdate final {
    ObjectId objectId;
    SceneOrderKey key;
    RecordHandle handle;
    bool remove = false;
};

struct PreparedDelta final {
    SceneDelta sceneDelta;
    std::vector<SlotWrite> slotWrites;
    std::vector<ObjectIndexUpdate> objectIndexUpdates;
    std::vector<OrderIndexUpdate> orderIndexUpdates;
};

[[nodiscard]] SceneDelta makeSceneDelta(const CompiledSceneDelta& delta);

} // namespace canvas
