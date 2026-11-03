#pragma once

#include "canvas/scene/scene_types.hpp"

namespace canvas::scene {

// Derives bounded, renderer-neutral source chunks from canonical stroke data.
// The helper never recomputes brush geometry and is safe to call from both
// full and incremental RuntimeScene projection.
[[nodiscard]] std::vector<GeometryChunk> buildGeometryChunks(
    const semantic::ObjectContent& content);

} // namespace canvas::scene
