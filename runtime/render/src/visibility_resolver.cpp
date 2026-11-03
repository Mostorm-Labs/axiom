#include "canvas/render/visibility_resolver.hpp"

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

namespace {
foundation::WorldRect transformed(const foundation::WorldRect& bounds,
                                  const semantic::Transform2D& t) {
    const foundation::WorldPoint points[] = {
        {bounds.left, bounds.top}, {bounds.right, bounds.top},
        {bounds.left, bounds.bottom}, {bounds.right, bounds.bottom}};
    foundation::WorldRect result{};
    bool initialized = false;
    for (const auto point : points) {
        const foundation::WorldPoint transformedPoint{
            static_cast<float>(t.a * point.x + t.c * point.y + t.tx),
            static_cast<float>(t.b * point.x + t.d * point.y + t.ty)};
        const foundation::WorldRect one{transformedPoint.x, transformedPoint.y,
                                        transformedPoint.x, transformedPoint.y};
        result = initialized ? foundation::unionRects(result, one) : one;
        initialized = true;
    }
    return result;
}
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
            if (transformed(chunk.bounds, record->transform).intersects(frameState.worldViewport))
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
