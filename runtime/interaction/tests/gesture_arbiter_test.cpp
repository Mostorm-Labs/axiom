#include "canvas/interaction/gesture_arbiter.hpp"

#include <cassert>

int main() {
  canvas::interaction::GestureArbiter arbiter;
  const canvas::input::PointerKey pen{1, 1, 1};
  const canvas::input::PointerKey finger{1, 2, 1};
  assert(arbiter.claim(pen, canvas::interaction::GestureClaim::kInk));
  assert(!arbiter.claim(pen, canvas::interaction::GestureClaim::kViewport));
  assert(arbiter.claim(finger, canvas::interaction::GestureClaim::kViewport));
  assert(arbiter.owns(pen, canvas::interaction::GestureClaim::kInk));
  assert(arbiter.release(pen, canvas::interaction::GestureClaim::kInk));
  arbiter.cancel(finger);
  assert(arbiter.owner(finger) == canvas::interaction::GestureClaim::kNone);
  return 0;
}
