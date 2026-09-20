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
  const PointerKey c{1, 3, 1};
  const PointerKey d{1, 4, 1};
  assert(multiInk.update(sample(c, PointerPhase::kDown, 40, 0, 3)) == ContactDisposition::kInk);
  assert(multiInk.update(sample(d, PointerPhase::kDown, 60, 0, 4)) == ContactDisposition::kInk);
  assert(multiInk.activeCount() == 4);
  assert(multiInk.update(sample(c, PointerPhase::kCancel, 40, 0, 5)) == ContactDisposition::kInk);
  assert(multiInk.activeCount() == 3);

  MultiContactCoordinator priority(MultiContactPolicy::kGesturePriority);
  assert(priority.update(sample(a, PointerPhase::kDown, 0, 0, 1)) == ContactDisposition::kPending);
  assert(priority.update(sample(a, PointerPhase::kMove, 20, 0, 2)) == ContactDisposition::kInk);
  assert(priority.update(sample(b, PointerPhase::kDown, 0, 0, 3)) == ContactDisposition::kIgnored);
  assert(priority.update(sample(a, PointerPhase::kMove, 30, 0, 4)) == ContactDisposition::kInk);

  MultiContactCoordinator spatial;
  assert(spatial.update(sample({3, 1, 1}, PointerPhase::kDown, 0, 0, 1)) == ContactDisposition::kPending);
  assert(spatial.update(sample({3, 1, 1}, PointerPhase::kMove, 3, 4, 2)) == ContactDisposition::kPending);
  assert(spatial.update(sample({3, 1, 1}, PointerPhase::kMove, 8, 0, 3)) == ContactDisposition::kInk);

  MultiContactCoordinator predicted;
  auto predictedDown = sample({2, 1, 1}, PointerPhase::kDown, 0, 0, 1);
  predictedDown.predicted = true;
  assert(predicted.update(predictedDown) == ContactDisposition::kIgnored);
  return 0;
}
