#pragma once

#include "canvas/render/frame_state.hpp"
#include "canvas/render/editing_overlay.hpp"

#include <utility>

namespace canvas::render {

// Minimal G3-01 holder for the state captured by one view.
class RenderViewRuntime final {
  public:
    explicit constexpr RenderViewRuntime(FrameState frameState) noexcept
        : frameState_(std::move(frameState)), editingOverlay_(frameState_) {}

    [[nodiscard]] constexpr const FrameState& frameState() const noexcept {
        return frameState_;
    }
    [[nodiscard]] EditingOverlay& editingOverlay() noexcept { return editingOverlay_; }
    [[nodiscard]] const EditingOverlay& editingOverlay() const noexcept { return editingOverlay_; }

  private:
    const FrameState frameState_;
    EditingOverlay editingOverlay_;
};

} // namespace canvas::render
