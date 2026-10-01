#include "canvas/input/platform_interaction_ingress.hpp"

#include <cassert>

using namespace canvas::input;

int main() {
  PlatformInteractionIngress ingress;
  PlatformPointerBatch batch;
  batch.samples = {
      {1, 1, 1, 100, 10.0F, 20.0F, 0.5F, 0.0F, 0.0F, {},
       {.tool = PointerTool::kFinger, .coalescedHistory = true},
       SampleProvenance::kConfirmedCurrent, PointerPhase::kDown},
      {1, 2, 2, 100, 30.0F, 40.0F, 0.5F, 0.0F, 0.0F, {},
       {.tool = PointerTool::kFinger, .coalescedHistory = true},
       SampleProvenance::kConfirmedCurrent, PointerPhase::kDown},
      {1, 1, 3, 200, 12.0F, 22.0F, 0.5F, 0.0F, 0.0F, {}, {},
       SampleProvenance::kCoalescedHistory, PointerPhase::kMove},
      {1, 2, 4, 200, 32.0F, 42.0F, 0.5F, 0.0F, 0.0F, {}, {},
       SampleProvenance::kPredicted, PointerPhase::kMove},
  };
  const auto result = ingress.submit(batch);
  assert(result.accepted && result.began == 2);
  assert(result.normalized.samples.size() == 4);
  assert(result.normalized.samples[2].key.generation == 1);
  assert(result.normalized.samples[3].predicted);
  const auto cancel = ingress.sourceLost(1, 5, 300);
  assert(cancel.terminalCancel && cancel.ended == 2 && ingress.activeCount() == 0);
  return 0;
}
