#pragma once

#include "canvas/render/frame_state.hpp"

#include <utility>

namespace canvas::render {

// Minimal G3-01 holder for the state captured by one view.
class RenderViewRuntime final {
  public:
    explicit constexpr RenderViewRuntime(FrameState frameState) noexcept
        : frameState_(std::move(frameState)) {}

    [[nodiscard]] constexpr const FrameState& frameState() const noexcept {
        return frameState_;
    }

  private:
    const FrameState frameState_;
};

} // namespace canvas::render
