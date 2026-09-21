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
  MultiContactCoordinator priorityPending(MultiContactPolicy::kGesturePriority);
  assert(priorityPending.update(sample(a, PointerPhase::kDown, 0, 0, 1)) == ContactDisposition::kPending);
  assert(priorityPending.update(sample(b, PointerPhase::kDown, 20, 0, 2)) == ContactDisposition::kViewportGesture);
  assert(priorityPending.disposition(a) == ContactDisposition::kViewportGesture);
  assert(priority.update(sample(a, PointerPhase::kDown, 0, 0, 1)) == ContactDisposition::kPending);
  assert(priority.update(sample(a, PointerPhase::kMove, 20, 0, 2)) == ContactDisposition::kInk);
  assert(priority.update(sample(b, PointerPhase::kDown, 0, 0, 3)) == ContactDisposition::kIgnored);
  assert(priority.update(sample(a, PointerPhase::kMove, 30, 0, 4)) == ContactDisposition::kInk);

  MultiContactCoordinator spatial;
  assert(spatial.update(sample({3, 1, 1}, PointerPhase::kDown, 0, 0, 1)) == ContactDisposition::kPending);
  assert(spatial.update(sample({3, 1, 1}, PointerPhase::kMove, 3, 4, 2)) == ContactDisposition::kPending);
  assert(spatial.update(sample({3, 1, 1}, PointerPhase::kMove, 8, 0, 3)) == ContactDisposition::kInk);

  MultiContactCoordinator conflict;
  const PointerKey conflictA{4, 1, 1};
  const PointerKey conflictB{4, 2, 1};
  assert(conflict.update(sample(conflictA, PointerPhase::kDown, 0, 0, 1)) == ContactDisposition::kPending);
  assert(conflict.update(sample(conflictA, PointerPhase::kMove, 20, 0, 2)) == ContactDisposition::kInk);
  assert(conflict.update(sample(conflictB, PointerPhase::kDown, 20, 0, 3)) == ContactDisposition::kInk);
  assert(!conflict.viewportClaimed());

  MultiContactCoordinator terminal;
  const PointerKey unknown{5, 1, 1};
  assert(terminal.update(sample(unknown, PointerPhase::kUp, 0, 0, 1)) == ContactDisposition::kIgnored);
  assert(terminal.activeCount() == 0);

  MultiContactCoordinator reusable;
  const PointerKey reusableA{6, 1, 1};
  const PointerKey reusableB{6, 2, 1};
  const PointerKey reusableC{6, 3, 1};
  const PointerKey reusableD{6, 4, 1};
  assert(reusable.update(sample(reusableA, PointerPhase::kDown, 0, 0, 1)) == ContactDisposition::kPending);
  assert(reusable.update(sample(reusableB, PointerPhase::kDown, 20, 0, 2)) == ContactDisposition::kViewportGesture);
  assert(reusable.update(sample(reusableA, PointerPhase::kUp, 0, 0, 3)) == ContactDisposition::kViewportGesture);
  assert(reusable.update(sample(reusableB, PointerPhase::kUp, 20, 0, 4)) == ContactDisposition::kViewportGesture);
  assert(!reusable.viewportClaimed());
  assert(reusable.update(sample(reusableC, PointerPhase::kDown, 0, 20, 5)) == ContactDisposition::kPending);
  assert(reusable.update(sample(reusableD, PointerPhase::kDown, 20, 20, 6)) == ContactDisposition::kViewportGesture);

  MultiContactCoordinator predicted;
  auto predictedDown = sample({2, 1, 1}, PointerPhase::kDown, 0, 0, 1);
  predictedDown.predicted = true;
  assert(predicted.update(predictedDown) == ContactDisposition::kIgnored);
  return 0;
}
