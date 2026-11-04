#include "canvas/render/visibility_resolver.hpp"
#include "affine_bounds.hpp"

#include "canvas/scene/scene.hpp"
#include "canvas/scene/scene_types.hpp"

#include <algorithm>
#include <cmath>
#include <utility>

namespace canvas::render {

foundation::Result<VisibilityResult>
VisibilityResolver::resolve(const FrameState& frameState, const Scene& scene) {
    auto queried = scene.query(SceneQuery{frameState.worldViewport});
    if (!queried) {
        return foundation::Result<VisibilityResult>::failure(queried.error());
    }

    SceneQueryResult result = std::move(queried.value());
    if (result.revision != frameState.sceneReadToken) {
        return foundation::Result<VisibilityResult>::failure(foundation::Error{
            foundation::ErrorCode::kInvalidRevision,
            "Visibility query revision does not match the FrameState read token",
        });
    }

    return foundation::Result<VisibilityResult>::success(VisibilityResult{
        .sceneGeneration = frameState.sceneGeneration,
        .sceneReadToken = result.revision,
        .queryWorldRect = frameState.worldViewport,
        .candidatesExamined = result.diagnostics.candidatesExamined,
        .visibleRecords = result.diagnostics.visibleRecords,
        .backToFront = std::move(result.backToFront),
    });
}

foundation::Result<VisibilityResult>
VisibilityResolver::resolve(const FrameState& frameState, const Scene& scene,
                            const RuntimeScene& runtimeScene) {
    auto base = resolve(frameState, scene);
    if (!base) return base;
    auto result = std::move(base.value());
    result.chunkSelections.reserve(result.backToFront.size());
    for (const auto objectId : result.backToFront) {
        const auto* record = runtimeScene.find(objectId);
        if (record == nullptr || record->geometryChunks.empty()) continue;
        VisibilityChunkSelection selection{.objectId = objectId, .chunkIndices = {}, .fullObject = false};
        for (const auto& chunk : record->geometryChunks) {
            if (internal::transformedBounds(chunk.bounds, record->transform).intersects(frameState.worldViewport))
                selection.chunkIndices.push_back(chunk.index);
        }
        if (selection.chunkIndices.empty()) {
            // The object itself is visible through conservative bounds, but
            // chunk metadata may be local/incomplete. Keep the full object.
            selection.fullObject = true;
        } else if (selection.chunkIndices.size() == record->geometryChunks.size()) {
            selection.fullObject = true;
        }
        result.chunkSelections.push_back(std::move(selection));
    }
    return foundation::Result<VisibilityResult>::success(std::move(result));
}

} // namespace canvas::render
