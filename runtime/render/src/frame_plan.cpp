#include "canvas/render/frame_plan.hpp"

namespace canvas::render {

foundation::Result<FramePlan> FramePlanBuilder::build(
    const FrameState& frame,
    const ReferenceDrawList& referenceDrawList,
    const foundation::DocumentBackground& background) {
    if (!(referenceDrawList.frame == frame) || !foundation::validDocumentBackground(background)) {
        return foundation::Result<FramePlan>::failure(foundation::Error{
            foundation::ErrorCode::kInvalidArgument,
            "FrameState and ReferenceDrawList identity mismatch",
        });
    }
    return foundation::Result<FramePlan>::success(FramePlan{
        .frame = frame,
        .referenceDrawList = referenceDrawList,
        .background = background,
    });
}

} // namespace canvas::render
