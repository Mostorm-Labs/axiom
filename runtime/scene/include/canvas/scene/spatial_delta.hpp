#pragma once

#include "canvas/scene/scene_delta.hpp"
#include "canvas/scene/spatial_index.hpp"

namespace canvas {

[[nodiscard]] SpatialDelta makeSpatialDelta(const SceneDelta& delta);

} // namespace canvas
