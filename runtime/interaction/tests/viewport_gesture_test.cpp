#include "canvas/interaction/viewport_gesture.hpp"

#include <cassert>
#include <limits>

namespace {
void coincidentContactsWaitForDistanceWithoutLosingGesture() {
  canvas::interaction::TwoFingerViewportGesture gesture;
  const canvas::input::PointerKey a{1, 1, 1};
  const canvas::input::PointerKey b{1, 2, 1};
  const auto update = [&](float ax, float ay, float bx, float by) {
    return gesture.update({1, 10, ax, ay, 0.0F, false, a},
                          {1, 10, bx, by, 0.0F, false, b});
  };
  assert(update(10.0F, 20.0F, 10.0F, 20.0F));
  assert(update(15.0F, 25.0F, 15.0F, 25.0F));
  assert(gesture.state().scale == 1.0F);
  assert(gesture.state().translationX == 0.0F);
  assert(gesture.state().translationY == 0.0F);
  // The first separated pair establishes the baseline without a camera jump.
  assert(update(10.0F, 20.0F, 30.0F, 20.0F));
  assert(gesture.state().scale == 1.0F);
  assert(gesture.state().translationX == 0.0F);
  assert(gesture.state().translationY == 0.0F);
  assert(update(15.0F, 25.0F, 55.0F, 25.0F));
  assert(gesture.state().scale == 2.0F);
  assert(gesture.state().translationX == -5.0F);
  assert(gesture.state().translationY == -15.0F);
  // A later overlap holds the last valid transform, rather than collapsing it.
  assert(update(35.0F, 25.0F, 35.0F, 25.0F));
  assert(gesture.state().scale == 2.0F);
  assert(gesture.state().translationX == -5.0F);
  assert(gesture.state().translationY == -15.0F);
  assert(update(20.0F, 30.0F, 80.0F, 30.0F));
  assert(gesture.state().scale == 3.0F);
  assert(gesture.state().translationX == -10.0F);
  assert(gesture.state().translationY == -30.0F);
  assert(!update(std::numeric_limits<float>::quiet_NaN(), 0.0F, 20.0F, 0.0F));
  assert(gesture.state().scale == 3.0F);
  gesture.reset();
  assert(update(5.0F, 5.0F, 5.0F, 5.0F));
  assert(update(0.0F, 0.0F, 10.0F, 0.0F));
  assert(gesture.state().scale == 1.0F);
}
}  // namespace

int main() {
  coincidentContactsWaitForDistanceWithoutLosingGesture();
  canvas::interaction::TwoFingerViewportGesture gesture;
  const canvas::input::PointerKey a{1, 1, 1};
  const canvas::input::PointerKey b{1, 2, 1};
  assert(gesture.update({1, 10, 0.0F, 0.0F, 0.0F, false, a},
                        {1, 10, 10.0F, 0.0F, 0.0F, false, b}));
  assert(gesture.state().centerX == 5.0F);
  assert(gesture.update({2, 20, 0.0F, 0.0F, 0.0F, false, a},
                        {2, 20, 20.0F, 0.0F, 0.0F, false, b}));
  assert(gesture.state().scale == 2.0F);
  assert(gesture.state().translationX == 0.0F);
  assert(gesture.state().translationY == 0.0F);
  assert(gesture.update({3, 30, 5.0F, 7.0F, 0.0F, false, a},
                        {3, 30, 25.0F, 7.0F, 0.0F, false, b}));
  assert(gesture.state().scale == 2.0F);
  assert(gesture.state().translationX == 5.0F);
  assert(gesture.state().translationY == 7.0F);
  canvas::interaction::ContactHysteresis palm(20.0F, 10.0F);
  assert(!palm.update({3.0F, 3.0F, 0.0F}));
  assert(palm.update({5.0F, 5.0F, 0.0F}));
  assert(palm.update({3.0F, 4.0F, 0.0F}));
  assert(!palm.update({2.0F, 4.0F, 0.0F}));
  return 0;
}
