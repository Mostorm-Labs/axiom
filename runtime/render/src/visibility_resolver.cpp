#include "canvas/render/visibility_resolver.hpp"

#include "canvas/scene/scene.hpp"

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

} // namespace canvas::render
