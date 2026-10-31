#pragma once

#include "canvas/foundation/result.hpp"
#include "canvas/foundation/document_background.hpp"
#include "canvas/render/frame_state.hpp"
#include "canvas/render/reference_draw_list.hpp"

namespace canvas::render {

// Gate-local immutable-by-contract orchestration value. This is neither a
// FrameGraph ABI nor a platform surface contract.
struct FramePlan final {
    const FrameState frame;
    const ReferenceDrawList referenceDrawList;
    const foundation::DocumentBackground background{};

    bool operator==(const FramePlan&) const = default;
};

class FramePlanBuilder final {
  public:
    [[nodiscard]] static foundation::Result<FramePlan> build(
        const FrameState& frame,
        const ReferenceDrawList& referenceDrawList,
        const foundation::DocumentBackground& background = {});
};

} // namespace canvas::render
