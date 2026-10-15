#include "minimal_runtime_host.hpp"

#include <cassert>

using canvas::interaction::DispatchMode;
using canvas::input::PlatformPointerBatch;
using canvas::input::PlatformPointerSample;
using canvas::input::PointerPhase;
using canvas::input::SampleProvenance;

static PlatformPointerBatch down(std::uint64_t sequence) {
  PlatformPointerBatch batch;
  batch.samples.push_back(PlatformPointerSample{
      9U, 3U, sequence, sequence * 1000U, 10.0F, 20.0F, 0.0F,
      0.0F, 0.0F, {}, {}, SampleProvenance::kConfirmedCurrent,
      PointerPhase::kDown});
  return batch;
}

static PlatformPointerBatch up(std::uint64_t sequence, std::uint64_t pointer) {
  PlatformPointerBatch batch;
  batch.samples.push_back(PlatformPointerSample{
      9U, pointer, sequence, sequence * 1000U, 10.0F, 20.0F, 0.0F,
      0.0F, 0.0F, {}, {}, SampleProvenance::kConfirmedCurrent,
      PointerPhase::kUp});
  return batch;
}

int main() {
  canvas::minimal_runtime_host::MinimalRuntimeHost host;
  assert(host.registerSource(9U));
  host.setMode(DispatchMode::kSelection);
  assert(host.submit(down(1U)));
  assert(host.eventCount(canvas::interaction::DispatchEvent::kSelection) == 1U);
  assert(host.submit(up(2U, 3U)));
  host.setMode(DispatchMode::kCreation);
  auto creationDown = down(3U);
  creationDown.samples.front().pointer = 4U;
  assert(host.submit(creationDown));
  assert(host.eventCount(canvas::interaction::DispatchEvent::kCreation) == 1U);
  return 0;
}
