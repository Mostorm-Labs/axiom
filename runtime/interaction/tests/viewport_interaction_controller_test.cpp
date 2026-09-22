#include "canvas/interaction/viewport_interaction_controller.hpp"

#include <cassert>
#include <cmath>

using namespace canvas::interaction;

int main() {
  ViewportInteractionController controller;
  const canvas::input::PointerKey a{1, 1, 1};
  const canvas::input::PointerKey b{1, 2, 1};
  assert(controller.updateGesture({1, 1, 0.0F, 0.0F, 0.0F, false, a},
                                  {1, 1, 20.0F, 0.0F, 0.0F, false, b}));
  assert(controller.state().scale == 1.0F);
  assert(controller.updateGesture({2, 2, 0.0F, 0.0F, 0.0F, false, a},
                                  {2, 2, 40.0F, 0.0F, 0.0F, false, b}));
  assert(controller.state().scale == 2.0F);
  controller.endGesture();
  assert(controller.committed().scale == 2.0F);
  assert(controller.applyNavigation({ViewportNavigationKind::kWheelPan, 5.0F, -3.0F, 0.0F, 0.0F}));
  assert(controller.state().translationX == -5.0F);
  assert(controller.state().translationY == 3.0F);
  assert(controller.applyNavigation({ViewportNavigationKind::kCtrlWheelZoom, 10.0F, -100.0F, 10.0F, 10.0F}));
  assert(controller.state().scale > 2.0F);
  const auto before = controller.viewToContent(10.0F, 10.0F);
  assert(controller.applyNavigation({ViewportNavigationKind::kBrowserGesture, 10.0F, 10.0F, 0.0F, 0.5F}));
  const auto after = controller.viewToContent(10.0F, 10.0F);
  assert(std::abs(before.first - after.first) < 1e-4F);
  controller.reset();
  assert(controller.state().scale == 1.0F);
  return 0;
}
