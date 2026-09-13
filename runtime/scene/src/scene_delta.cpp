#include "canvas/scene/scene_delta.hpp"

namespace canvas {

SceneDelta makeSceneDelta(const CompiledSceneDelta& delta) {
    SceneDelta result{
        .generationFrom = delta.beforeRevision,
        .generationTo = delta.afterRevision,
        .addedRecords = {},
        .updatedRecords = {},
        .removedHandles = {},
        .orderChanges = {},
        .boundsChanges = {},
        .mutations = delta.mutations,
    };
    result.addedRecords.reserve(delta.mutations.size());
    result.updatedRecords.reserve(delta.mutations.size());
    result.removedHandles.reserve(delta.mutations.size());
    result.orderChanges.reserve(delta.mutations.size());
    result.boundsChanges.reserve(delta.mutations.size());
    for (const SceneMutation& mutation : delta.mutations) {
        if (mutation.kind == SceneMutationKind::kInsert && mutation.after) {
            result.addedRecords.push_back(*mutation.after);
        } else if (mutation.kind == SceneMutationKind::kUpdate) {
            result.updatedRecords.push_back(mutation);
        } else if (mutation.kind == SceneMutationKind::kRemove) {
            result.removedHandles.push_back(0);
        }
        if (mutation.before && mutation.after &&
            mutation.before->orderKey != mutation.after->orderKey) {
            result.orderChanges.push_back(SceneOrderChange{
                mutation.objectId, mutation.before->orderKey, mutation.after->orderKey});
        }
        if (mutation.before && mutation.after &&
            mutation.before->worldBounds != mutation.after->worldBounds) {
            result.boundsChanges.push_back(SceneBoundsChange{
                mutation.objectId, mutation.before->worldBounds, mutation.after->worldBounds});
        }
    }
    return result;
}

} // namespace canvas
