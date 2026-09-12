#pragma once

#include "canvas/scene/runtime_scene_projection.hpp"

namespace canvas::scene {

class FullSceneCompiler final {
  public:
    [[nodiscard]] static foundation::Result<RuntimeSceneProjection> compile(
        const semantic::SemanticReadView& view);
};

} // namespace canvas::scene

namespace canvas { using scene::FullSceneCompiler; }
