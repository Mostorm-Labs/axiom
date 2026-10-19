#include "canvas/input/platform_interaction_ingress.hpp"
#include <cassert>
using namespace canvas::input;
int main() {
  PlatformInteractionIngress ingress;
  PlatformPointerBatch a{};
  a.samples.push_back({11, 7, 1, 100, 10, 20, 0.4F, 0, 0, {},
                       {.tool = PointerTool::kPen, .pressure = true},
                       SampleProvenance::kConfirmedCurrent, PointerPhase::kDown});
  PlatformPointerBatch b{};
  b.samples.push_back({22, 8, 1, 100, 30, 40, 0.4F, 0, 0, {},
                       {.tool = PointerTool::kFinger, .contactGeometry = true},
                       SampleProvenance::kConfirmedCurrent, PointerPhase::kDown});
  assert(ingress.submit(a).accepted);
  const auto b_down = ingress.submit(b);
  assert(b_down.accepted && b_down.normalized.samples.size() == 1);
  const auto b_key = b_down.normalized.samples.front().key;
  assert(ingress.activeCount() == 2);
  const auto lost_a = ingress.sourceLost(11, 2, 200);
  assert(lost_a.accepted);
  assert(ingress.activeCount() == 1);
  assert(ingress.accepts(b_key) && ingress.keyFor(22, 8) == b_key);
  assert(!lost_a.terminalCancel && !lost_a.normalized.terminalCancel);
  assert(lost_a.normalized.samples.size() == 1);
  assert(lost_a.normalized.samples.front().key.pointer == 7);
  assert(lost_a.normalized.samples.front().phase == PointerPhase::kCancel);
  const auto b_move = ingress.submit(PlatformPointerBatch{{
      {22, 8, 2, 300, 35, 45, 0.5F, 0, 0, {}, {},
       SampleProvenance::kConfirmedCurrent, PointerPhase::kMove}}, false, false});
  assert(b_move.accepted && b_move.normalized.samples.size() == 1);
  assert(b_move.normalized.samples.front().key == b_key);
  const auto b_up = ingress.submit(PlatformPointerBatch{{
      {22, 8, 3, 400, 35, 45, 0.0F, 0, 0, {}, {},
       SampleProvenance::kConfirmedCurrent, PointerPhase::kUp}}, false, false});
  assert(b_up.accepted && b_up.ended == 1 && ingress.activeCount() == 0);
  assert(b_up.normalized.samples.size() == 1);
  assert(b_up.normalized.samples.front().key == b_key);
  assert(b_up.normalized.samples.front().phase == PointerPhase::kUp);
  const auto unknown_loss = ingress.sourceLost(99, 4, 500);
  assert(!unknown_loss.accepted && !unknown_loss.terminalCancel);
  return 0;
}
