#include "canvas/input/platform_interaction_ingress.hpp"

#include <cassert>

using namespace canvas::input;

int main() {
  // Reject the entire mixed-source batch before admitting either Down.
  {
    PlatformInteractionIngress mixedIngress;
    PlatformPointerBatch mixed;
    PlatformPointerSample a{11, 7, 1, 100}; a.phase = PointerPhase::kDown;
    auto b = a; b.source = 12;
    mixed.samples = {a, b};
    const auto rejected = mixedIngress.submit(mixed);
    assert(!rejected.accepted && rejected.normalized.samples.empty());
    assert(mixedIngress.activeCount() == 0U);
  }
  PlatformInteractionIngress ingress;
  assert(ingress.captureGate().onBatchBoundary() == InputCaptureDecision::kPassThrough);
  assert(ingress.captureGate().onTerminalBoundary() == InputCaptureDecision::kRelease);

  PlatformPointerBatch down{};
  down.samples.push_back({11, 7, 1, 100, 10, 20, 0.4F, 1.0F, 2.0F, {},
                          {.tool = PointerTool::kFinger, .coalescedHistory = true},
                          SampleProvenance::kConfirmedCurrent, PointerPhase::kDown});
  assert(down.samples.front().modifiers == PointerModifiers::kNone);
  down.samples.front().modifiers = PointerModifiers::kShift;
  const auto first = ingress.submit(down);
  assert(first.accepted && first.began == 1 && first.normalized.samples.size() == 1);
  const auto key = first.normalized.samples.front().key;
  assert(key.valid() && first.normalized.samples.front().capabilities.coalescedHistory);
  assert(first.normalized.samples.front().modifiers == PointerModifiers::kShift);

  PlatformPointerBatch move{};
  move.samples.push_back({11, 7, 2, 200, 11, 21, 0.5F, 1.0F, 2.0F, {}, {},
                          SampleProvenance::kCoalescedHistory, PointerPhase::kMove});
  const auto second = ingress.submit(move);
  assert(second.accepted && second.normalized.samples.front().key == key);
  assert(second.normalized.samples.front().modifiers == PointerModifiers::kNone);

  PlatformPointerBatch up{};
  up.samples.push_back({11, 7, 3, 300, 12, 22, 0.6F, 1.0F, 2.0F, {}, {},
                        SampleProvenance::kConfirmedCurrent, PointerPhase::kUp});
  const auto third = ingress.submit(up);
  assert(third.accepted && third.ended == 1 && ingress.activeCount() == 0);

  down.samples.front().sequence = 3;
  down.samples.front().timestampNs = 350;
  const auto reused = ingress.submit(down);
  assert(reused.normalized.samples.front().key.generation == key.generation + 1);

  // A stale cancellation retains its full generation and must not end the
  // freshly reused contact.
  const auto staleCancel = ingress.cancelPointer(key, 4, 400);
  assert(!staleCancel.accepted && ingress.activeCount() == 1);
  const auto exactCancel = ingress.cancelPointer(reused.normalized.samples.front().key, 5, 500);
  assert(exactCancel.accepted && exactCancel.ended == 1 && ingress.activeCount() == 0);

  PlatformPointerBatch stale{};
  stale.samples.push_back({11, 7, 2, 250, 99, 99, 0.6F, 0.0F, 0.0F, {}, {},
                           SampleProvenance::kConfirmedCurrent, PointerPhase::kMove});
  const auto staleResult = ingress.submit(stale);
  assert(!staleResult.accepted && staleResult.normalized.samples.empty());

  const auto lost = ingress.sourceLost(11, 6, 600);
  assert(!lost.accepted && !lost.terminalCancel && ingress.activeCount() == 0);
  return 0;
}
