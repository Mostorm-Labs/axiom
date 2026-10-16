#include "minimal_runtime_host.hpp"
#include "canvas/interaction/creation_session.hpp"

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

template<class Host>
static void realEditingWithoutHostToolCallbacks(Host& host) {
  if constexpr (requires { host.runtime(); }) {
    auto& runtime = host.runtime();
    assert(runtime.bindSurface(256, 256));
    assert(runtime.selectCreationPreset(canvas::interaction::CreationPreset::kRectangle));
    auto d = down(10U);
    d.samples.front().x = 20; d.samples.front().y = 20;
    assert(host.submit(d));
    auto m = d; m.samples.front().phase = PointerPhase::kMove;
    m.samples.front().sequence = 11; m.samples.front().x = 120; m.samples.front().y = 100;
    assert(host.submit(m));
    auto u = m; u.samples.front().phase = PointerPhase::kUp; u.samples.front().sequence = 12;
    assert(host.submit(u));
    assert(runtime.semanticObjectCount() == 1U);
    assert(runtime.submittedOperationCount() == 1U);
    assert(runtime.setSelectionMode(true));
    d.samples.front().sequence = 13; d.samples.front().x = 60; d.samples.front().y = 60;
    assert(host.submit(d));
    u=d; u.samples.front().sequence=14; u.samples.front().phase=PointerPhase::kUp;
    assert(host.submit(u));
    assert(runtime.selectedObjectCount() == 1U);
    assert(runtime.submittedOperationCount() == 1U);
    const auto before=runtime.selectedObjectRecord(); assert(before);
    d.samples.front().sequence=15;
    assert(host.submit(d));
    m=d; m.samples.front().sequence=16; m.samples.front().phase=PointerPhase::kMove;
    m.samples.front().x=90;
    assert(host.submit(m));
    u=m; u.samples.front().sequence=17; u.samples.front().phase=PointerPhase::kUp;
    assert(host.submit(u));
    const auto moved=runtime.selectedObjectRecord(); assert(moved);
    assert(moved->transform.tx == before->transform.tx + 30);
    assert(runtime.canUndo()); assert(runtime.undo());
    const auto restored=runtime.selectedObjectRecord(); assert(restored);
    assert(restored->transform == before->transform);
    assert(runtime.redo());
    const auto redone=runtime.selectedObjectRecord(); assert(redone);
    assert(redone->transform == moved->transform);
  } else {
    assert(false && "Minimal Host must expose real reusable editing, not event counters");
  }
}

int main() {
  canvas::minimal_runtime_host::MinimalRuntimeHost realHost;
  assert(realHost.registerSource(9U));
  realEditingWithoutHostToolCallbacks(realHost);
  return 0;
}
