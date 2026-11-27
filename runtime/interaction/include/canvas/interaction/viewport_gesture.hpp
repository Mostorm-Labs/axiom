#pragma once

#include "canvas/input/pointer_sample.hpp"
#include "canvas/interaction/viewport_constraints.hpp"

#include <cmath>
#include <numeric>

namespace canvas::interaction {

struct ViewportGesture final {
  float centerX = 0.0F;
  float centerY = 0.0F;
  float scale = 1.0F;
  float translationX = 0.0F;
  float translationY = 0.0F;
};

class TwoFingerViewportGesture final {
 public:
  bool update(const input::PointerSample& first, const input::PointerSample& second,
              const ViewportGesture& startingCamera = {}) noexcept {
    if (!first.key.valid() || !second.key.valid() || first.key == second.key) return false;
    const double distance = std::hypot(double(second.x) - first.x, double(second.y) - first.y);
    if (!std::isfinite(distance)) return false;
    // Coincident contacts hold the last camera until a usable distance is available.
    if (!active_) state_ = startingCamera;
    if (distance == 0) return true;
    const float centerX = std::midpoint(first.x, second.x);
    const float centerY = std::midpoint(first.y, second.y);
    if (!active_) {
      baselineDistance_ = distance;
      baselineCenterX_ = centerX;
      baselineCenterY_ = centerY;
      active_ = true;
    }
    const double requested = startingCamera.scale * (distance / baselineDistance_);
    const float zoom = clampViewportZoom(requested);
    const double ratio = double(zoom) / startingCamera.scale;
    // Discard overshoot so the next inward movement takes effect immediately.
    // Keep the original center anchor even while the fingers pan at the limit.
    if (requested < kMinViewportZoom || requested > kMaxViewportZoom)
      baselineDistance_ = distance / ratio;
    state_ = {centerX, centerY, zoom,
              static_cast<float>(centerX - ratio * (double(baselineCenterX_) - startingCamera.translationX)),
              static_cast<float>(centerY - ratio * (double(baselineCenterY_) - startingCamera.translationY))};
    return true;
  }
  void reset() noexcept {
    active_ = false;
    baselineDistance_ = 0.0F;
    baselineCenterX_ = 0.0F;
    baselineCenterY_ = 0.0F;
    state_ = {};
  }
  [[nodiscard]] const ViewportGesture& state() const noexcept { return state_; }

 private:
  ViewportGesture state_{};
  double baselineDistance_ = 0;
  float baselineCenterX_ = 0.0F;
  float baselineCenterY_ = 0.0F;
  bool active_ = false;
};

class ContactHysteresis final {
 public:
  explicit ContactHysteresis(float enterArea, float exitArea) noexcept
      : enterArea_(enterArea), exitArea_(exitArea) {}
  [[nodiscard]] bool update(const input::ContactGeometry& contact) noexcept {
    if (!active_ && contact.area() >= enterArea_) active_ = true;
    else if (active_ && contact.area() <= exitArea_) active_ = false;
    return active_;
  }
  [[nodiscard]] bool active() const noexcept { return active_; }

 private:
  float enterArea_ = 0.0F;
  float exitArea_ = 0.0F;
  bool active_ = false;
};

}  // namespace canvas::interaction
