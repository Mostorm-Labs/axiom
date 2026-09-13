#include "canvas/scene/spatial_delta.hpp"

namespace canvas {

SpatialDelta makeSpatialDelta(const SceneDelta& delta) {
    SpatialDelta result{.generationFrom = delta.generationFrom,
                        .generationTo = delta.generationTo,
                        .mutations = {}};
    result.mutations.reserve(delta.mutations.size());
    for (const auto& mutation : delta.mutations) {
        result.mutations.push_back(SpatialMutation{
            .kind = mutation.kind,
            .objectId = mutation.objectId,
            .before = mutation.before ? std::optional(mutation.before->worldBounds) : std::nullopt,
            .after = mutation.after ? std::optional(mutation.after->worldBounds) : std::nullopt,
        });
    }
    return result;
}

} // namespace canvas
