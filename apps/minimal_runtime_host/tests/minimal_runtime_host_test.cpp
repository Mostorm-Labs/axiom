#include "minimal_runtime_host.hpp"
#include "minimal_runtime_facade.hpp"
#include "canvas/interaction/creation_session.hpp"

#include <cassert>
#include <thread>

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
  {
    auto& runtime = host.composition().runtime();
    assert(host.composition().surfaceSnapshot().available);
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
  }
}

int main() {
  canvas::minimal_runtime_host::MinimalRuntimeHost realHost;
  assert(realHost.composition().bindSurface(256U, 256U));
  assert(realHost.registerSource(9U));
  realEditingWithoutHostToolCallbacks(realHost);

  canvas::minimal_runtime_host::MinimalRuntimeHost facadeHost;
  canvas::minimal_runtime_host::MinimalRuntimeFacade facade(facadeHost);
  assert(facadeHost.composition().bindSurface(256, 256));
  const auto target = facade.mountedCanvasTargets().front();
  assert(facade.setBrush(2U, 1U, 1U, target.runtimeEpoch).state ==
         canvas::runtime::ProductControlState::kQueued);
  assert(facade.process() == 1U);
  assert(facadeHost.composition().runtime().selectedBrushProfile() == "marker-flat-v1");

  assert(facade.setSelectionMode(true, 2U, target.runtimeEpoch).state ==
         canvas::runtime::ProductControlState::kQueued);
  assert(facade.process() == 1U);
  assert(facadeHost.composition().runtime().selectionMode());

  assert(facade.setEraser(2U, 3U, target.runtimeEpoch).state ==
         canvas::runtime::ProductControlState::kQueued);
  assert(facade.process() == 1U);
  assert(facadeHost.composition().runtime().toolMode() ==
         canvas::interaction::CanvasEditingRuntime::ToolMode::kPartialEraser);

  const auto creation = facade.submitCanvasControl([&] {
    canvas::runtime::CanvasControlRequest request{};
    request.clientId = 1U;
    request.requestId = 4U;
    request.target = target;
    request.payload.kind = canvas::runtime::CanvasControlPayloadKind::kSelectCreationPreset;
    request.payload.creationPreset = canvas::runtime::CanvasCreationPreset::kEllipse;
    return request;
  }());
  assert(creation.state == canvas::runtime::ProductControlState::kQueued);
  assert(facade.process() == 1U);
  assert(facadeHost.composition().runtime().creationPreset() ==
         canvas::interaction::CreationPreset::kEllipse);

  // The facade's public pump must share Composition's owner clock; a second
  // counter must not roll the deadline domain back after prior frame pumps.
  canvas::minimal_runtime_host::MinimalRuntimeHost clockHost;
  canvas::minimal_runtime_host::MinimalRuntimeFacade clockFacade(clockHost);
  assert(clockHost.composition().bindSurface(256U, 256U));
  for (std::uint64_t i = 0; i < 300U; ++i) assert(clockHost.composition().ownerPump());
  assert(clockFacade.process() == 0U);
  canvas::runtime::CanvasControlRequest rectangleRequest{};
  rectangleRequest.clientId = 0xD001U;
  rectangleRequest.requestId = 9U;
  rectangleRequest.target = clockFacade.target();
  rectangleRequest.payload.kind =
      canvas::runtime::CanvasControlPayloadKind::kSelectCreationPreset;
  rectangleRequest.payload.creationPreset =
      canvas::runtime::CanvasCreationPreset::kRectangle;
  const auto rectangle = clockFacade.submitCanvasControl(rectangleRequest);
  assert(rectangle.state == canvas::runtime::ProductControlState::kQueued);
  assert(clockHost.composition().ownerPump());
  assert(clockFacade.canvasControlReceipt(rectangle.control->key)->state ==
         canvas::runtime::CanvasControlReceiptState::kApplied);
  assert(clockHost.composition().runtime().creationMode());

  // Public mutation ports reject non-owner calls before touching the runtime.
  canvas::runtime::RuntimeComposition threadFence;
  assert(threadFence.open() && threadFence.bindSurface(256U, 256U));
  bool resized = true;
  bool registered = true;
  std::thread worker([&] {
    resized = threadFence.resizeSurface(300U, 200U);
    registered = threadFence.registerSource(77U);
  });
  worker.join();
  assert(!resized && !registered);
  assert(threadFence.surfaceSnapshot().width == 256U);
  return 0;
}
