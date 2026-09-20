#pragma once

#include "canvas/input/pointer_sample.hpp"

#include <cmath>

namespace canvas::interaction {

struct ViewportGesture final {
  float centerX = 0.0F;
  float centerY = 0.0F;
  float scale = 1.0F;
};

class TwoFingerViewportGesture final {
 public:
  bool update(const input::PointerSample& first, const input::PointerSample& second) noexcept {
    if (!first.key.valid() || !second.key.valid() || first.key == second.key) return false;
    const float dx = second.x - first.x;
    const float dy = second.y - first.y;
    const float distance = std::sqrt(dx * dx + dy * dy);
    if (distance <= 0.0F) return false;
    if (!active_) { baselineDistance_ = distance; active_ = true; }
    state_ = { (first.x + second.x) * 0.5F, (first.y + second.y) * 0.5F,
                distance / baselineDistance_ };
    return true;
  }
  void reset() noexcept { active_ = false; baselineDistance_ = 0.0F; state_ = {}; }
  [[nodiscard]] const ViewportGesture& state() const noexcept { return state_; }

 private:
  ViewportGesture state_{};
  float baselineDistance_ = 0.0F;
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
