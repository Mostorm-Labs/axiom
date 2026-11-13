#pragma once

#include "canvas/foundation/result.hpp"
#include "canvas/render/reference_draw_list.hpp"
#include "canvas/render/visibility_result.hpp"

namespace canvas::render {

class DirectReferenceSource final {
  public:
    [[nodiscard]] static foundation::Result<ReferenceCommand> presentationCommand(const RuntimeSceneRecord&);
    [[nodiscard]] static foundation::Result<ReferenceDrawList> build(
        const FrameState& frame,
        const VisibilityResult& visibility,
        const RuntimeScene& runtimeScene);
    // Complete frame-local sources, before temporary transforms/masks are
    // applied. Unlike navigation this never culls or truncates commands; unlike
    // canonical build it produces no serialized bytes or canonical digest.
    [[nodiscard]] static foundation::Result<ReferenceDrawList> buildTransient(
        const FrameState&, const VisibilityResult&, const RuntimeScene&);
    // Navigation has no canonical digest/receipt. It shares command mapping
    // without serializing every confirmed sample on each tile request.
    [[nodiscard]] static foundation::Result<ReferenceDrawList> buildNavigation(
        const FrameState&,const VisibilityResult&,const RuntimeScene&);
};

} // namespace canvas::render
