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
    // Navigation has no canonical digest/receipt. It shares command mapping
    // without serializing every confirmed sample on each tile request.
    [[nodiscard]] static foundation::Result<ReferenceDrawList> buildNavigation(
        const FrameState&,const VisibilityResult&,const RuntimeScene&);
};

} // namespace canvas::render
