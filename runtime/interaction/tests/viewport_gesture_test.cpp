#include "canvas/interaction/viewport_gesture.hpp"

#include <cassert>

int main() {
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
