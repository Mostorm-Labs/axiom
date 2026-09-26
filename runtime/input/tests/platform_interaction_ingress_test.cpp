#include "canvas/input/platform_interaction_ingress.hpp"

#include <cassert>

using namespace canvas::input;

int main() {
  PlatformInteractionIngress ingress;
  assert(ingress.captureGate().onBatchBoundary() == InputCaptureDecision::kPassThrough);
  assert(ingress.captureGate().onTerminalBoundary() == InputCaptureDecision::kRelease);

  PlatformPointerBatch down{};
  down.samples.push_back({11, 7, 1, 100, 10, 20, 0.4F, 1.0F, 2.0F, {},
                          {.tool = PointerTool::kFinger, .coalescedHistory = true},
                          SampleProvenance::kConfirmedCurrent, PointerPhase::kDown});
  const auto first = ingress.submit(down);
  assert(first.accepted && first.began == 1 && first.normalized.samples.size() == 1);
  const auto key = first.normalized.samples.front().key;
  assert(key.valid() && first.normalized.samples.front().capabilities.coalescedHistory);

  PlatformPointerBatch move{};
  move.samples.push_back({11, 7, 2, 200, 11, 21, 0.5F, 1.0F, 2.0F, {}, {},
                          SampleProvenance::kCoalescedHistory, PointerPhase::kMove});
  const auto second = ingress.submit(move);
  assert(second.accepted && second.normalized.samples.front().key == key);

  PlatformPointerBatch up{};
  up.samples.push_back({11, 7, 3, 300, 12, 22, 0.6F, 1.0F, 2.0F, {}, {},
                        SampleProvenance::kConfirmedCurrent, PointerPhase::kUp});
  const auto third = ingress.submit(up);
  assert(third.accepted && third.ended == 1 && ingress.activeCount() == 0);

  const auto reused = ingress.submit(down);
  assert(reused.normalized.samples.front().key.generation == key.generation + 1);

  const auto lost = ingress.sourceLost(11, 4, 400);
  assert(lost.accepted && lost.terminalCancel && ingress.activeCount() == 0);
  return 0;
}
