#include "canvas/interaction/viewport_interaction_controller.hpp"

#include <algorithm>
#include <cmath>

namespace canvas::interaction {

bool ViewportInteractionController::updateGesture(
    const input::PointerSample& first, const input::PointerSample& second) noexcept {
  if (!gesture_.update(first, second)) return false;
  const auto relative = gesture_.state();
  state_.centerX = relative.centerX;
  state_.centerY = relative.centerY;
  state_.scale = relative.scale * committed_.scale;
  state_.translationX = relative.scale * committed_.translationX + relative.translationX;
  state_.translationY = relative.scale * committed_.translationY + relative.translationY;
  return true;
}

void ViewportInteractionController::endGesture() noexcept {
  committed_ = state_;
  gesture_.reset();
}

bool ViewportInteractionController::applyNavigation(
    const ViewportNavigationSample& sample) noexcept {
  switch (sample.kind) {
    case ViewportNavigationKind::kWheelPan:
      state_.translationX -= sample.deltaX;
      state_.translationY -= sample.deltaY;
      committed_ = state_;
      return true;
    case ViewportNavigationKind::kCtrlWheelZoom: {
      const float factor = std::exp(-sample.deltaY * 0.01F);
      const float next = std::clamp(state_.scale * factor, 0.25F, 8.0F);
      const float ratio = next / state_.scale;
      state_.translationX = sample.anchorX - ratio * (sample.anchorX - state_.translationX);
      state_.translationY = sample.anchorY - ratio * (sample.anchorY - state_.translationY);
      state_.scale = next;
      committed_ = state_;
      return true;
    }
    case ViewportNavigationKind::kBrowserGesture: {
      if (!std::isfinite(sample.scaleDelta) || sample.scaleDelta <= 0.0F) return false;
      const float next = std::clamp(state_.scale * sample.scaleDelta, 0.25F, 8.0F);
      const float ratio = next / state_.scale;
      state_.translationX = sample.anchorX - ratio * (sample.anchorX - state_.translationX);
      state_.translationY = sample.anchorY - ratio * (sample.anchorY - state_.translationY);
      state_.scale = next;
      committed_ = state_;
      return true;
    }
  }
  return false;
}

std::pair<float, float> ViewportInteractionController::viewToContent(
    float x, float y) const noexcept {
  if (state_.scale <= 0.0F || !std::isfinite(state_.scale)) return {x, y};
  return {(x - state_.translationX) / state_.scale,
          (y - state_.translationY) / state_.scale};
}

void ViewportInteractionController::reset() noexcept {
  gesture_.reset();
  state_ = {};
  committed_ = {};
  state_.scale = 1.0F;
  committed_.scale = 1.0F;
}

}  // namespace canvas::interaction
