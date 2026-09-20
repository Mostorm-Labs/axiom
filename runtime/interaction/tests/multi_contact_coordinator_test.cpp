#include "canvas/interaction/multi_contact_coordinator.hpp"

#include <cassert>

using canvas::input::PointerKey;
using canvas::input::PointerPhase;
using canvas::input::PointerSample;
using canvas::interaction::ContactDisposition;
using canvas::interaction::MultiContactCoordinator;
using canvas::interaction::MultiContactPolicy;

PointerSample sample(PointerKey key, PointerPhase phase, float x, float y,
                    std::uint64_t sequence) {
  return PointerSample{sequence, sequence, x, y, 0.0F, false, key, {}, {}, phase};
}

int main() {
  const PointerKey a{1, 1, 1};
  const PointerKey b{1, 2, 1};
  MultiContactCoordinator autoIntent;
  assert(autoIntent.update(sample(a, PointerPhase::kDown, 0, 0, 1)) == ContactDisposition::kPending);
  assert(autoIntent.update(sample(b, PointerPhase::kDown, 20, 0, 2)) == ContactDisposition::kViewportGesture);
  assert(autoIntent.viewportClaimed());
  assert(!autoIntent.canonicalMutation());

  MultiContactCoordinator inkAfterFirst;
  assert(inkAfterFirst.update(sample(a, PointerPhase::kDown, 0, 0, 1)) == ContactDisposition::kPending);
  assert(inkAfterFirst.update(sample(a, PointerPhase::kMove, 20, 0, 2)) == ContactDisposition::kInk);
  assert(inkAfterFirst.update(sample(b, PointerPhase::kDown, 20, 20, 3)) == ContactDisposition::kInk);

  MultiContactCoordinator positionOnly;
  auto maskedA = sample(a, PointerPhase::kDown, 0, 0, 1);
  auto maskedB = sample(b, PointerPhase::kDown, 20, 0, 2);
  maskedA.capabilities = {};
  maskedB.capabilities = {};
  maskedA.contact = {};
  maskedB.contact = {};
  assert(positionOnly.update(maskedA) == ContactDisposition::kPending);
  assert(positionOnly.update(maskedB) == ContactDisposition::kViewportGesture);

  MultiContactCoordinator multiInk(MultiContactPolicy::kMultiInk);
  assert(multiInk.update(sample(a, PointerPhase::kDown, 0, 0, 1)) == ContactDisposition::kInk);
  assert(multiInk.update(sample(b, PointerPhase::kDown, 20, 0, 2)) == ContactDisposition::kInk);
  assert(!multiInk.viewportClaimed());
  assert(multiInk.activeCount() == 2);

  MultiContactCoordinator priority(MultiContactPolicy::kGesturePriority);
  assert(priority.update(sample(a, PointerPhase::kDown, 0, 0, 1)) == ContactDisposition::kPending);
  assert(priority.update(sample(a, PointerPhase::kMove, 20, 0, 2)) == ContactDisposition::kInk);
  assert(priority.update(sample(b, PointerPhase::kDown, 0, 0, 3)) == ContactDisposition::kIgnored);
  assert(priority.update(sample(a, PointerPhase::kMove, 30, 0, 4)) == ContactDisposition::kInk);

  MultiContactCoordinator predicted;
  auto predictedDown = sample({2, 1, 1}, PointerPhase::kDown, 0, 0, 1);
  predictedDown.predicted = true;
  assert(predicted.update(predictedDown) == ContactDisposition::kIgnored);
  return 0;
}
