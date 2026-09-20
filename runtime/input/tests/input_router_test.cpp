#include "canvas/input/input_router.hpp"

#include <cassert>

int main() {
  canvas::input::InputRouter router;
  canvas::input::PointerSampleBatch batch;
  batch.samples.push_back({1, 10, 2.0F, 3.0F, 0.5F, false});
  batch.samples.push_back({2, 20, 4.0F, 5.0F, 0.8F, true});
  assert(router.dispatch(batch) == canvas::input::DispatchDisposition::kDelivered);
  assert(router.confirmedCount() == 1);
  assert(router.predictedCount() == 1);
  assert(router.lastConfirmedSequence() == 1);
  canvas::input::PointerSampleBatch concurrent;
  concurrent.samples.push_back({1, 30, 1.0F, 1.0F, 0.5F, false, {9, 1, 1}});
  concurrent.samples.push_back({1, 31, 2.0F, 2.0F, 0.5F, false, {9, 2, 1}});
  assert(router.dispatch(concurrent) == canvas::input::DispatchDisposition::kDelivered);
  assert(router.lastConfirmedSequence({9, 1, 1}) == 1);
  assert(router.lastConfirmedSequence({9, 2, 1}) == 1);
  canvas::input::PointerSampleBatch stale;
  stale.samples.push_back({1, 32, 3.0F, 3.0F, 0.5F, false, {9, 1, 1}});
  assert(router.dispatch(stale) == canvas::input::DispatchDisposition::kRejected);
  return 0;
}
