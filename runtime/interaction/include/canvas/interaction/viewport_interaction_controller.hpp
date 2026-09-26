#pragma once

#include "canvas/interaction/viewport_gesture.hpp"

#include <utility>

namespace canvas::interaction {

enum class ViewportNavigationKind : std::uint8_t {
  kWheelPan,
  kCtrlWheelZoom,
  kBrowserGesture,
};

struct ViewportNavigationSample final {
  ViewportNavigationKind kind = ViewportNavigationKind::kWheelPan;
  float deltaX = 0.0F;
  float deltaY = 0.0F;
  float anchorX = 0.0F;
  float anchorY = 0.0F;
  float scaleDelta = 1.0F;
};

class ViewportInteractionController final {
 public:
  [[nodiscard]] bool updateGesture(const input::PointerSample& first,
                                   const input::PointerSample& second) noexcept;
  void endGesture() noexcept;
  [[nodiscard]] bool applyNavigation(const ViewportNavigationSample& sample) noexcept;
  [[nodiscard]] const ViewportGesture& state() const noexcept { return state_; }
  [[nodiscard]] const ViewportGesture& committed() const noexcept { return committed_; }
  [[nodiscard]] std::pair<float, float> viewToContent(float x, float y) const noexcept;
  void reset() noexcept;

 private:
  TwoFingerViewportGesture gesture_;
  ViewportGesture state_{};
  ViewportGesture committed_{};
};

}  // namespace canvas::interaction
