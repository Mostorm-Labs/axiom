#include "../common/canvas_control_binding.hpp"
#include "canvas/input/platform_input_contract.hpp"
#include "ink_playground_history_test_access.hpp"
#include "canvas_runtime_facade_adapter.hpp"

#include <cassert>
#include <thread>
#include <limits>
#include <iostream>

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

void arrow_then_second_creation_remains_usable() {
  InkPlaygroundHost host;
  assert(host.bindSurface(256U, 256U));
  const CanvasTargetKey target{1U, 1U, 1U, 1U};
  CanvasControlBinding binding(host, target);
  auto select = [&](CanvasCreationPreset preset, std::uint64_t requestId) {
    CanvasControlRequest request{};
    request.clientId = 7U;
    request.requestId = requestId;
    request.target = target;
    request.payload.kind = CanvasControlPayloadKind::kSelectCreationPreset;
    request.payload.creationPreset = preset;
    assert(binding.apply(request).error == CanvasControlError::kNone);
  };
  auto draw = [&](std::uint64_t pointer, float x0, float y0, float x1, float y1) {
    PlatformPointerBatch down;
    down.samples.push_back({11U, pointer, 1U, 1000U, x0, y0, 0.5F, 0.0F, 0.0F,
                            {}, {}, SampleProvenance::kConfirmedCurrent, PointerPhase::kDown});
    PlatformPointerBatch move;
    move.samples.push_back({11U, pointer, 2U, 2000U, x1, y1, 0.5F, 0.0F, 0.0F,
                            {}, {}, SampleProvenance::kConfirmedCurrent, PointerPhase::kMove});
    PlatformPointerBatch up;
    up.samples.push_back({11U, pointer, 3U, 3000U, x1, y1, 0.5F, 0.0F, 0.0F,
                          {}, {}, SampleProvenance::kConfirmedCurrent, PointerPhase::kUp});
    assert(host.acceptPlatformBatch(down, 1000U));
    assert(host.acceptPlatformBatch(move, 2000U));
    assert(host.acceptPlatformBatch(up, 3000U));
  };
  select(CanvasCreationPreset::kArrow, 1U);
  draw(71U, 20.0F, 30.0F, 120.0F, 30.0F);
  assert(host.semanticObjectCount() == 1U);
  assert(host.presentCanonicalFrame(host.nextCanonicalFrameId(), 0.0));
  select(CanvasCreationPreset::kRectangle, 2U);
  draw(72U, 40.0F, 60.0F, 160.0F, 140.0F);
  assert(host.semanticObjectCount() == 2U &&
         "creating an Arrow must not block the next creation gesture");
}

void arrow_then_arrow_remains_usable() {
  InkPlaygroundHost host;
  assert(host.bindSurface(256U, 256U));
  const CanvasTargetKey target{1U, 1U, 1U, 1U};
  CanvasControlBinding binding(host, target);
  auto selectArrow = [&](std::uint64_t requestId) {
    CanvasControlRequest request{};
    request.clientId = 7U;
    request.requestId = requestId;
    request.target = target;
    request.payload.kind = CanvasControlPayloadKind::kSelectCreationPreset;
    request.payload.creationPreset = CanvasCreationPreset::kArrow;
    assert(binding.apply(request).error == CanvasControlError::kNone);
  };
  auto draw = [&](std::uint64_t pointer, float x0, float y0, float x1, float y1) {
    PlatformPointerBatch down;
    down.samples.push_back({11U, pointer, 1U, 1000U, x0, y0, 0.5F, 0.0F, 0.0F,
                            {}, {}, SampleProvenance::kConfirmedCurrent, PointerPhase::kDown});
    PlatformPointerBatch move;
    move.samples.push_back({11U, pointer, 2U, 2000U, x1, y1, 0.5F, 0.0F, 0.0F,
                            {}, {}, SampleProvenance::kConfirmedCurrent, PointerPhase::kMove});
    PlatformPointerBatch up;
    up.samples.push_back({11U, pointer, 3U, 3000U, x1, y1, 0.5F, 0.0F, 0.0F,
                          {}, {}, SampleProvenance::kConfirmedCurrent, PointerPhase::kUp});
    assert(host.acceptPlatformBatch(down, 1000U));
    assert(host.acceptPlatformBatch(move, 2000U));
    assert(host.acceptPlatformBatch(up, 3000U));
  };
  selectArrow(1U);
  draw(81U, 20.0F, 30.0F, 120.0F, 30.0F);
  assert(host.semanticObjectCount() == 1U);
  assert(host.presentCanonicalFrame(host.nextCanonicalFrameId(), 0.0));
  // The product keeps the selected creation preset active after a commit;
  // a second gesture must therefore work without another control request.
  draw(82U, 40.0F, 60.0F, 160.0F, 60.0F);
  assert(host.semanticObjectCount() == 2U &&
         "creating an Arrow must not block another Arrow gesture");
}

void arrow_then_arrow_reuses_platform_pointer() {
  InkPlaygroundHost host;
  assert(host.bindSurface(256U, 256U));
  assert(host.selectCreationPreset(CreationPreset::kArrow));
  auto draw = [&](std::uint64_t sequence, float y) {
    PlatformPointerBatch down;
    down.samples.push_back({11U, 1U, sequence, sequence * 1000U, 20.0F, y,
                            0.5F, 0.0F, 0.0F, {}, {},
                            SampleProvenance::kConfirmedCurrent, PointerPhase::kDown});
    PlatformPointerBatch move;
    move.samples.push_back({11U, 1U, sequence + 1U, (sequence + 1U) * 1000U,
                            120.0F, y, 0.5F, 0.0F, 0.0F, {}, {},
                            SampleProvenance::kConfirmedCurrent, PointerPhase::kMove});
    PlatformPointerBatch up;
    up.samples.push_back({11U, 1U, sequence + 2U, (sequence + 2U) * 1000U,
                          120.0F, y, 0.5F, 0.0F, 0.0F, {}, {},
                          SampleProvenance::kConfirmedCurrent, PointerPhase::kUp});
    assert(host.acceptPlatformBatch(down, sequence * 1000U));
    assert(host.acceptPlatformBatch(move, (sequence + 1U) * 1000U));
    assert(host.acceptPlatformBatch(up, (sequence + 2U) * 1000U));
  };
  draw(1U, 30.0F);
  assert(host.semanticObjectCount() == 1U);
  assert(host.presentCanonicalFrame(host.nextCanonicalFrameId(), 0.0));
  draw(4U, 60.0F);
  assert(host.semanticObjectCount() == 2U &&
         "a stable Windows mouse pointer must be reusable for Arrow creation");
}

void line_then_line_reuses_platform_pointer() {
  InkPlaygroundHost host;
  assert(host.bindSurface(256U, 256U));
  assert(host.selectCreationPreset(CreationPreset::kLine));
  auto draw = [&](std::uint64_t sequence, float y) {
    PlatformPointerBatch down;
    down.samples.push_back({11U, 1U, sequence, sequence * 1000U, 20.0F, y,
                            0.5F, 0.0F, 0.0F, {}, {},
                            SampleProvenance::kConfirmedCurrent, PointerPhase::kDown});
    PlatformPointerBatch move;
    move.samples.push_back({11U, 1U, sequence + 1U, (sequence + 1U) * 1000U,
                            120.0F, y, 0.5F, 0.0F, 0.0F, {}, {},
                            SampleProvenance::kConfirmedCurrent, PointerPhase::kMove});
    PlatformPointerBatch up;
    up.samples.push_back({11U, 1U, sequence + 2U, (sequence + 2U) * 1000U,
                          120.0F, y, 0.5F, 0.0F, 0.0F, {}, {},
                          SampleProvenance::kConfirmedCurrent, PointerPhase::kUp});
    assert(host.acceptPlatformBatch(down, sequence * 1000U));
    assert(host.acceptPlatformBatch(move, (sequence + 1U) * 1000U));
    assert(host.acceptPlatformBatch(up, (sequence + 2U) * 1000U));
  };
  draw(1U, 30.0F);
  assert(host.semanticObjectCount() == 1U);
  assert(host.presentCanonicalFrame(host.nextCanonicalFrameId(), 0.0));
  draw(4U, 60.0F);
  assert(host.semanticObjectCount() == 2U &&
         "Line must remain reusable for a second connector gesture");
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
  assert(host.presentTransientOverlayFrame(host.nextCanonicalFrameId()));
  assert(host.previewSurfaceProvider()->readbackRgba(preview).code ==
         canvas::render::BackendSubmissionCode::kAccepted);
  assert(preview != std::vector<std::uint8_t>(preview.size(), 0U) &&
         "creation Move must render real transient pixels");
  assert(host.semanticGeneration() == generation && host.semanticObjectCount() == 0U);
  assert(host.selectedObjectCount() == 0U);
  assert(host.creationPointer(key, PointerPhase::kCancel, 120, 100));
  assert(host.clearTransientOverlay());
  assert(host.previewSurfaceProvider()->readbackRgba(preview).code ==
         canvas::render::BackendSubmissionCode::kAccepted);
  assert(preview == std::vector<std::uint8_t>(preview.size(), 0U));
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

void source_unregister_closes_coordinator_contact() {
  InkPlaygroundHost host; assert(host.bindSurface(256,256));
  assert(host.selectCreationPreset(CreationPreset::kRectangle));
  assert(host.registerInputSource(11, "test"));
  PlatformPointerBatch a;
  a.samples.push_back({11, 7, 1, 1000, 20, 20, 0.5F, 0, 0, {}, {},
                       SampleProvenance::kConfirmedCurrent, PointerPhase::kDown});
  assert(host.acceptPlatformBatch(a, 1000));
  assert(host.unregisterInputSource(11, 2, 2000));
  assert(!host.creationActive());
  assert(!host.transientOverlayDirty());
  assert(!host.acceptPlatformBatch(a, 3000));
  assert(host.registerInputSource(11, "test"));
  PlatformPointerBatch b;
  b.samples.push_back({11, 8, 3, 4000, 40, 40, 0.5F, 0, 0, {}, {},
                       SampleProvenance::kConfirmedCurrent, PointerPhase::kDown});
  assert(host.acceptPlatformBatch(b, 4000));
  assert(!host.viewportGestureClaimed());
}

void failed_brush_down_rolls_back_session() {
  InkPlaygroundHost host; assert(host.bindSurface(256,256));
  PlatformPointerBatch invalid;
  invalid.samples.push_back({11, 9, 1, 1000,
                             std::numeric_limits<float>::quiet_NaN(), 20.0F,
                             .5F, 0, 0, {}, {}, SampleProvenance::kConfirmedCurrent,
                             PointerPhase::kDown});
  assert(!host.acceptPlatformBatch(invalid, 1000));
  assert(host.setSelectionMode(true));
  assert(!host.previewActive());
}

void queued_width_patch_preserves_remote_style() {
  InkPlaygroundHost host; assert(host.bindSurface(256,256));
  assert(host.selectCreationPreset(CreationPreset::kRectangle));
  PlatformPointerBatch down;
  down.samples.push_back({11, 7, 1, 1000, 20, 20, .5F, 0, 0, {}, {},
                          SampleProvenance::kConfirmedCurrent, PointerPhase::kDown});
  PlatformPointerBatch up = down;
  up.samples.front().sequence = 2; up.samples.front().timestampNs = 2000;
  up.samples.front().phase = PointerPhase::kUp; up.samples.front().x = 100; up.samples.front().y = 80;
  assert(host.acceptPlatformBatch(down, 1000)); assert(host.acceptPlatformBatch(up, 2000));
  assert(host.setSelectionMode(true)); assert(host.selectAtViewPoint(50, 45));
  const CanvasTargetKey target{1,1,1,1}; CanvasControlBinding binding(host, target);
  CanvasControlService service(binding);
  const auto draft = binding.snapshot(target).selectedAppearance;
  using namespace canvas::semantic;
  const auto id = host.selectedPrimaryObject();
  const SolidStroke remote{{.9F,.1F,.2F,1},2,StrokeCap::kRound,RoundJoin{},DashPattern{{3,4},1}};
  CanvasControlRequest request{}; request.clientId=1; request.requestId=1; request.target=target;
  request.payload.kind=CanvasControlPayloadKind::kSelectedAppearance;
  request.payload.appearance=draft; request.payload.appearance.strokeWidth=9;
  assert(service.enqueue(request).state == CanvasControlReceiptState::kQueued);
  // The queue captures its base at enqueue. Interleave the remote edit after
  // that capture, so unchanged fields can be merged at the owner safe point.
  assert(canvas::ink_playground::InkPlaygroundHistoryTestAccess::applyRemote(host,
      PatchPropertiesOp{{{id,0x101,PropertyPatchAction::kSet,
          PropertyValue{StrokeStyleValue{remote}}}}}));
  assert(service.processPending(1) == 1);
  const auto actual = host.selectedObjectRecord(); assert(actual.has_value());
  const auto entry = std::find_if(actual->properties.entries.begin(), actual->properties.entries.end(),
      [](const auto& item){ return item.field_id == 0x101U; });
  assert(entry != actual->properties.entries.end());
  const auto result = std::get<SolidStroke>(std::get<StrokeStyleValue>(entry->value));
  assert(result.width == 9 && result.color == remote.color && result.cap == remote.cap &&
         result.join == remote.join && result.dash == remote.dash);
}

void queued_selected_size_keeps_original_target() {
  InkPlaygroundHost host; assert(host.bindSurface(256,256));
  const CanvasTargetKey target{1,1,1,1}; CanvasControlBinding binding(host, target);
  CanvasControlService service(binding);
  auto draw = [&](std::uint64_t pointer, float x, float y) {
    assert(host.selectCreationPreset(CreationPreset::kRectangle));
    PlatformPointerBatch d;
    d.samples.push_back({11,pointer,1,1000,x,y,.5F,0,0,{}, {},
                         SampleProvenance::kConfirmedCurrent,PointerPhase::kDown});
    PlatformPointerBatch u=d; u.samples.front().sequence=2; u.samples.front().timestampNs=2000;
    u.samples.front().phase=PointerPhase::kUp; u.samples.front().x=x+60; u.samples.front().y=y+50;
    assert(host.acceptPlatformBatch(d,1000)); assert(host.acceptPlatformBatch(u,2000));
  };
  draw(31,20,20); draw(32,160,20);
  assert(host.setSelectionMode(true)); assert(host.selectAtViewPoint(50,45));
  const auto first = host.selectedPrimaryObject();
  CanvasControlRequest request{}; request.clientId=9; request.requestId=1; request.target=target;
  request.payload.kind=CanvasControlPayloadKind::kSelectedSize; request.payload.width=120; request.payload.height=90;
  assert(service.enqueue(request).state == CanvasControlReceiptState::kQueued);
  assert(host.selectAtViewPoint(190,45));
  const auto second = host.selectedPrimaryObject(); assert(first != second);
  const auto before = canvas::ink_playground::InkPlaygroundHistoryTestAccess::objects(host);
  const auto operations = host.submittedOperationCount();
  assert(service.processPending(1) == 0); // Rejected before reaching Apply.
  const auto receipt = service.receipt({9,1}); assert(receipt.has_value());
  assert(receipt->state == CanvasControlReceiptState::kStaleTarget);
  assert(canvas::ink_playground::InkPlaygroundHistoryTestAccess::objects(host) == before);
  assert(host.submittedOperationCount() == operations);
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
  else if(argc>1 && std::string_view(argv[1])=="unregister") source_unregister_closes_coordinator_contact();
  else if(argc>1 && std::string_view(argv[1])=="width-style") queued_width_patch_preserves_remote_style();
  else if(argc>1 && std::string_view(argv[1])=="failed-down") failed_brush_down_rolls_back_session();
  else if(argc>1 && std::string_view(argv[1])=="target-capture") queued_selected_size_keeps_original_target();
  else if(argc>1 && std::string_view(argv[1])=="owner") queue_never_applies_off_owner();
  else if (argc > 1 && std::string_view(argv[1]) == "arrow") arrow_then_second_creation_remains_usable();
  else if (argc > 1 && std::string_view(argv[1]) == "arrow-arrow") arrow_then_arrow_remains_usable();
  else if (argc > 1 && std::string_view(argv[1]) == "arrow-arrow-pointer") arrow_then_arrow_reuses_platform_pointer();
  else if (argc > 1 && std::string_view(argv[1]) == "line-line-pointer") line_then_line_reuses_platform_pointer();
  else if(argc==1) one_gesture_one_operation();
  else return 64;
  std::cout << "PASS FC01 control scenario: " << (argc > 1 ? argv[1] : "one-gesture") << '\n';
  return 0;
}
