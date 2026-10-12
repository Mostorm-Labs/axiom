#include "../common/canvas_control_binding.hpp"
#include "canvas/input/platform_input_contract.hpp"
#include "ink_playground_history_test_access.hpp"
#include "canvas_runtime_facade_adapter.hpp"

#include <cassert>
#include <thread>

using namespace canvas::ink_playground;
using namespace canvas::runtime;
using namespace canvas::input;

void one_gesture_one_operation() {
  InkPlaygroundHost host;
  assert(host.bindSurface(320U, 240U));
  const CanvasTargetKey target{1U, 1U, 1U, 1U};
  CanvasControlBinding binding(host, target);
  CanvasControlRequest request{};
  request.clientId = 7U;
  request.requestId = 1U;
  request.target = target;
  request.payload.kind = CanvasControlPayloadKind::kSelectCreationPreset;
  request.payload.creationPreset = CanvasCreationPreset::kArrow;
  assert(binding.apply(request).error == CanvasControlError::kNone);
  const auto before = host.submittedOperationCount();
  PlatformPointerBatch down;
  down.samples.push_back({11U, 42U, 1U, 1000U, 20.0F, 30.0F, 0.5F, 0.0F, 0.0F,
                          {}, {}, SampleProvenance::kConfirmedCurrent, PointerPhase::kDown});
  PlatformPointerBatch move;
  move.samples.push_back({11U, 42U, 2U, 2000U, 120.0F, 30.0F, 0.5F, 0.0F, 0.0F,
                          {}, {}, SampleProvenance::kConfirmedCurrent, PointerPhase::kMove});
  PlatformPointerBatch up;
  up.samples.push_back({11U, 42U, 3U, 3000U, 120.0F, 30.0F, 0.5F, 0.0F, 0.0F,
                        {}, {}, SampleProvenance::kConfirmedCurrent, PointerPhase::kUp});
  assert(host.acceptPlatformBatch(down, 1000U));
  assert(host.acceptPlatformBatch(move, 2000U));
  assert(host.submittedOperationCount() == before);
  assert(host.acceptPlatformBatch(up, 3000U));
  assert(host.submittedOperationCount() == before + 1U);
  const auto snapshot = binding.snapshot(target);
  assert(snapshot.creationPreset == CanvasCreationPreset::kArrow);
  assert(!snapshot.creationActive);
}

void create_move_changes_real_pixels_without_canonical_mutation() {
  InkPlaygroundHost host;
  assert(host.bindSurface(256U, 256U));
  assert(host.presentCanonicalFrame(1U, 0.0, false));
  std::vector<std::uint8_t> before(256U * 256U * 4U), preview(before.size());
  assert(host.activeSurfaceProvider()->readbackRgba(before).code ==
         canvas::render::BackendSubmissionCode::kAccepted);
  const auto generation = host.semanticGeneration();
  assert(host.selectCreationPreset(CreationPreset::kTriangle));
  const PointerKey key{11U, 42U, 1U};
  assert(host.creationPointer(key, PointerPhase::kDown, 20, 20));
  assert(host.creationPointer(key, PointerPhase::kMove, 120, 100));
  assert(host.presentCanonicalFrame(2U, 0.0, false));
  assert(host.activeSurfaceProvider()->readbackRgba(preview).code ==
         canvas::render::BackendSubmissionCode::kAccepted);
  assert(preview != before && "creation Move must render real transient pixels");
  assert(host.semanticGeneration() == generation && host.semanticObjectCount() == 0U);
  assert(host.selectedObjectCount() == 0U);
  assert(host.creationPointer(key, PointerPhase::kCancel, 120, 100));
  assert(host.presentCanonicalFrame(3U, 0.0, false));
  assert(host.activeSurfaceProvider()->readbackRgba(preview).code ==
         canvas::render::BackendSubmissionCode::kAccepted);
  assert(preview == before);
}

void repeated_gestures_and_other_source_cancellation() {
  InkPlaygroundHost host;
  assert(host.bindSurface(256,256));
  assert(host.selectCreationPreset(CreationPreset::kRectangle));
  const auto send = [&](std::uint64_t source, std::uint64_t seq, PointerPhase phase,
                        float x, float y, bool lost=false) {
    PlatformPointerBatch b;
    b.samples.push_back({source,7U,seq,seq*1000U,x,y,0.5F,0,0,{}, {},
                        SampleProvenance::kConfirmedCurrent,phase}); b.sourceLost=lost;
    return host.acceptPlatformBatch(b,seq*1000U);
  };
  assert(send(11,1,PointerPhase::kDown,20,20));
  assert(send(12,2,PointerPhase::kDown,40,40));
  assert(send(12,3,PointerPhase::kCancel,40,40,true));
  assert(host.creationActive());
  assert(send(11,4,PointerPhase::kMove,100,80));
  assert(send(11,5,PointerPhase::kUp,100,80));
  assert(host.semanticObjectCount()==1);
  assert(send(11,6,PointerPhase::kDown,140,140));
  assert(send(11,7,PointerPhase::kMove,220,200));
  assert(send(11,8,PointerPhase::kUp,220,200));
  assert(host.semanticObjectCount()==2 && host.submittedOperationCount()==2);
}

struct Facade final : CanvasRuntimeFacadeAdapter {
  Facade(InkPlaygroundHost& h,CanvasTargetKey t):CanvasRuntimeFacadeAdapter(h,t){}
  RuntimeStateSnapshot readRuntimeState() const noexcept override {return {};}
  ProductControlReceipt submitProductControl(const ProductControlRequest&) noexcept override {return {};}
};
void queue_never_applies_off_owner() {
  InkPlaygroundHost host; assert(host.bindSurface(256,256));
  Facade facade(host,{1,1,1,1});
  CanvasControlRequest r{};r.target={1,1,1,1};r.clientId=19;r.requestId=1;
  r.payload.kind=CanvasControlPayloadKind::kSelectCreationPreset;
  r.payload.creationPreset=CanvasCreationPreset::kTriangle;
  assert(facade.submitCanvasControl(r).state==ProductControlState::kQueued);
  assert(host.creationPreset()==CreationPreset::kRectangle);
  std::size_t offOwner=99;
  std::thread worker([&]{offOwner=facade.processCanvasControls(1);});worker.join();
  assert(offOwner==0 && host.creationPreset()==CreationPreset::kRectangle);
  assert(facade.processCanvasControls(1)==1);
  assert(host.creationPreset()==CreationPreset::kTriangle);
  assert(host.semanticObjectCount()==0);
}
int main(int argc, char** argv) {
  if (argc > 1 && std::string_view(argv[1])=="preview") create_move_changes_real_pixels_without_canonical_mutation();
  else if(argc>1 && std::string_view(argv[1])=="sources") repeated_gestures_and_other_source_cancellation();
  else if(argc>1) queue_never_applies_off_owner();
  else one_gesture_one_operation();
  return 0;
}
