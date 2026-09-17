#pragma once

#include "canvas/foundation/result.hpp"
#include "canvas/render/frame_state.hpp"
#include "canvas/render/visibility_result.hpp"
#include "canvas/scene/scene_query.hpp"

namespace canvas {
class Scene;
}

namespace canvas::render {

class VisibilityResolver final {
  public:
    [[nodiscard]] static foundation::Result<VisibilityResult>
    resolve(const FrameState& frameState, const Scene& scene);
};

} // namespace canvas::render
