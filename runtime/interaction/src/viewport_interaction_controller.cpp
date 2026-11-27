#include "canvas/interaction/viewport_interaction_controller.hpp"

#include <cmath>

namespace canvas::interaction {

bool ViewportInteractionController::updateGesture(
    const input::PointerSample& first, const input::PointerSample& second) noexcept {
  if (!gesture_.update(first, second, committed_)) return false;
  const auto& next = gesture_.state();
  if (next.scale != state_.scale || next.translationX != state_.translationX ||
      next.translationY != state_.translationY) ++cameraGeneration_;
  state_ = next;
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
      gesture_.reset();
      committed_ = state_;
      ++cameraGeneration_;
      return true;
    case ViewportNavigationKind::kCtrlWheelZoom:
      if (!std::isfinite(sample.deltaY)) return false;
      return zoomAt(clampViewportZoom(state_.scale * std::exp(-double(sample.deltaY) * 0.01)),
                    sample.anchorX, sample.anchorY);
    case ViewportNavigationKind::kBrowserGesture:
      if (!std::isfinite(sample.scaleDelta) || sample.scaleDelta <= 0) return false;
      return zoomAt(clampViewportZoom(double(state_.scale) * sample.scaleDelta),
                    sample.anchorX, sample.anchorY);
  }
  return false;
}

bool ViewportInteractionController::zoomAt(float zoom, float anchorX, float anchorY) noexcept {
  if (!std::isfinite(zoom) || zoom <= 0 || !std::isfinite(anchorX) || !std::isfinite(anchorY)) return false;
  const float next = clampViewportZoom(zoom);
  if (next == state_.scale) return true;
  const double ratio = double(next) / state_.scale;
  return setCamera(next,
                   static_cast<float>(anchorX - ratio * (double(anchorX) - state_.translationX)),
                   static_cast<float>(anchorY - ratio * (double(anchorY) - state_.translationY)));
}

bool ViewportInteractionController::setCamera(float zoom, float translationX,
                                               float translationY) noexcept {
  if (!std::isfinite(zoom) || !std::isfinite(translationX) ||
      !std::isfinite(translationY) || zoom <= 0.0F) {
    return false;
  }
  zoom = clampViewportZoom(zoom);
  if (state_.scale == zoom && state_.translationX == translationX &&
      state_.translationY == translationY) {
    return true;
  }
  gesture_.reset();
  state_.scale = zoom;
  state_.translationX = translationX;
  state_.translationY = translationY;
  committed_ = state_;
  ++cameraGeneration_;
  return true;
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
  cameraGeneration_ = 1;
}

}  // namespace canvas::interaction
