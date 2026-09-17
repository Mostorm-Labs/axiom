#pragma once

#include "canvas/foundation/result.hpp"
#include "canvas/render/reference_draw_list.hpp"
#include "canvas/render/visibility_result.hpp"

namespace canvas::render {

class DirectReferenceSource final {
  public:
    [[nodiscard]] static foundation::Result<ReferenceDrawList> build(
        const FrameState& frame,
        const VisibilityResult& visibility,
        const RuntimeScene& runtimeScene);
};

} // namespace canvas::render
