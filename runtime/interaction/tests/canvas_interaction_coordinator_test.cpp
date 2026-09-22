#include "canvas/interaction/canvas_interaction_coordinator.hpp"

#include <cassert>

using namespace canvas::interaction;
using canvas::input::PointerKey;
using canvas::input::PointerPhase;
using canvas::input::PointerSample;

static PointerSample sample(PointerKey key, PointerPhase phase, float x, float y,
                            std::uint64_t sequence) {
  return PointerSample{sequence, sequence, x, y, 0.5F, false, key, {}, {}, phase};
}

int main() {
  CanvasInteractionCoordinator coordinator;
  const PointerKey a{1, 1, 1};
  const PointerKey b{1, 2, 1};
  auto first = coordinator.route(sample(a, PointerPhase::kDown, 0.0F, 0.0F, 1));
  assert(first.disposition == ContactDisposition::kPending);
  auto second = coordinator.route(sample(b, PointerPhase::kDown, 20.0F, 0.0F, 2));
  assert(second.viewportClaimed && second.becameViewport);
  assert(coordinator.viewportSamples().size() == 2U);
  assert(!coordinator.canonicalMutation());
  auto predicted = sample(a, PointerPhase::kMove, 60.0F, 0.0F, 3);
  predicted.predicted = true;
  assert(coordinator.route(predicted).disposition == ContactDisposition::kViewportGesture);
  auto upA = coordinator.route(sample(a, PointerPhase::kUp, 0.0F, 0.0F, 4));
  assert(upA.viewportClaimed && !upA.endedViewport);
  auto upB = coordinator.route(sample(b, PointerPhase::kUp, 20.0F, 0.0F, 5));
  assert(!upB.viewportClaimed && upB.endedViewport);
  assert(coordinator.viewportSamples().empty());

  coordinator.reset();
  assert(coordinator.route(sample(a, PointerPhase::kDown, 0.0F, 0.0F, 6)).disposition == ContactDisposition::kPending);
  assert(coordinator.route(sample(a, PointerPhase::kMove, 20.0F, 0.0F, 7)).disposition == ContactDisposition::kInk);
  assert(coordinator.route(sample(b, PointerPhase::kDown, 20.0F, 0.0F, 8)).disposition == ContactDisposition::kInk);
  assert(!coordinator.viewportClaimed());
  return 0;
}
