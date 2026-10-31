#include "canvas/runtime/runtime_composition.hpp"
#include <cassert>

int main() {
  using namespace canvas;
  runtime::RuntimeComposition owner;
  assert(owner.open());
  input::PlatformPointerSample sample{};
  sample.source=17; sample.pointer=1; sample.sequence=1; sample.timestampNs=1'000'000;
  sample.x=24; sample.y=32; sample.phase=input::PointerPhase::kDown;
  input::PlatformPointerBatch batch; batch.samples.push_back(sample);
  assert(!owner.beginContact(batch)); // A surface is required by the public ingress.
  assert(owner.bindSurface(256,256) && owner.registerSource(17));
  auto& facade=owner.controlFacade();
  runtime::CanvasControlRequest request{};
  request.target=facade.mountedCanvasTargets().front(); request.clientId=74; request.requestId=1;
  request.payload.kind=runtime::CanvasControlPayloadKind::kSelectTool;
  request.payload.tool=runtime::CanvasToolKind::kSelect;
  assert(facade.submitCanvasControl(request).state==runtime::ProductControlState::kQueued);
  assert(owner.ownerPump());
  const auto receipt=facade.canvasControlReceipt({74,1});
  assert(receipt && receipt->state==runtime::CanvasControlReceiptState::kApplied);
  const auto before=owner.canonicalCommitFence(); assert(before);
  const auto key=owner.beginContact(batch); assert(key);
  sample.sequence=2; sample.timestampNs=2'000'000; sample.x=30; sample.y=38;
  sample.phase=input::PointerPhase::kUp; batch.samples[0]=sample;
  assert(owner.submitContact(*key,batch));
  assert(owner.canonicalCommitFence()==before);
  assert(owner.hostObservation()->objectCount==0);
  const auto state=facade.readCanvasControlSnapshot(facade.mountedCanvasTargets().front());
  assert(state.tool==runtime::CanvasToolKind::kSelect && !state.canUndo);
  assert(owner.close());
}
