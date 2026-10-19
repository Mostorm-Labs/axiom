#include "../common/canvas_control_binding.hpp"
#include "canvas/input/platform_input_contract.hpp"

#include <cassert>

using namespace canvas::ink_playground;
using namespace canvas::runtime;
using namespace canvas::input;

int main() {
  InkPlaygroundHost host;
  assert(host.bindSurface(400, 300));
  CanvasControlBinding binding(host, {1, 1, 1, 1});
  const auto beforeOps = host.submittedOperationCount();

  CanvasControlRequest zoom{};
  zoom.clientId = 1; zoom.requestId = 1; zoom.target = {1, 1, 1, 1};
  zoom.payload.kind = CanvasControlPayloadKind::kZoomAt;
  zoom.payload.x = 200.0F; zoom.payload.y = 150.0F; zoom.payload.scale = 2.0F;
  assert(binding.apply(zoom).error == CanvasControlError::kNone);
  assert(host.viewportGesture().scale == 2.0F);

  CanvasControlRequest pan = zoom;
  pan.requestId = 2; pan.payload.kind = CanvasControlPayloadKind::kPanBy;
  pan.payload.x = 10.0F; pan.payload.y = -4.0F;
  assert(binding.apply(pan).error == CanvasControlError::kNone);
  assert(host.viewportGesture().translationX != 0.0F);
  assert(host.submittedOperationCount() == beforeOps);

  CanvasControlRequest fit = zoom;
  fit.requestId = 3; fit.payload.kind = CanvasControlPayloadKind::kFit;
  fit.payload.fitTarget = CanvasFitTarget::kContent;
  // An empty document has no finite content bounds. The accepted camera
  // policy is a truthful no-op rejection rather than inventing a fit target.
  const auto beforeFit = host.viewportGesture();
  assert(binding.apply(fit).error == CanvasControlError::kBusy);
  assert(host.viewportGesture().scale == beforeFit.scale);
  assert(host.viewportGesture().translationX == beforeFit.translationX);
  assert(host.viewportGesture().translationY == beforeFit.translationY);
  CanvasControlRequest world = fit;
  world.requestId = 4;
  world.payload.fitTarget = CanvasFitTarget::kWorldRect;
  world.payload.worldRect = {0.0F, 0.0F, 128.0F, 96.0F};
  assert(binding.apply(world).error == CanvasControlError::kNone);
  assert(host.viewportGesture().scale > 0.0F);

  // Exercise the actual platform pointer ingress used by Windows/Web/Android,
  // rather than only the typed camera request. Pan must claim the contact,
  // update the camera, and never create a brush/canonical operation.
  CanvasControlRequest selectPan = zoom;
  selectPan.requestId = 5;
  selectPan.payload.kind = CanvasControlPayloadKind::kSelectTool;
  selectPan.payload.tool = CanvasToolKind::kPan;
  assert(binding.apply(selectPan).error == CanvasControlError::kNone);
  const auto beforePointerPan = host.viewportGesture();
  const auto beforePointerOps = host.submittedOperationCount();
  PlatformPointerBatch down;
  down.samples.push_back({10, 1, 10, 1'000, 100.0F, 100.0F, 0.0F, 0.0F, 0.0F,
                          {}, {}, SampleProvenance::kConfirmedCurrent,
                          PointerPhase::kDown});
  PlatformPointerBatch move;
  move.samples.push_back({10, 1, 11, 2'000, 124.0F, 91.0F, 0.0F, 0.0F, 0.0F,
                          {}, {}, SampleProvenance::kConfirmedCurrent,
                          PointerPhase::kMove});
  PlatformPointerBatch up;
  up.samples.push_back({10, 1, 12, 3'000, 124.0F, 91.0F, 0.0F, 0.0F, 0.0F,
                        {}, {}, SampleProvenance::kConfirmedCurrent,
                        PointerPhase::kUp});
  assert(host.acceptPlatformBatch(down, 1'000));
  assert(host.acceptPlatformBatch(move, 2'000));
  assert(host.acceptPlatformBatch(up, 3'000));
  assert(host.viewportGesture().translationX != beforePointerPan.translationX ||
         host.viewportGesture().translationY != beforePointerPan.translationY);
  assert(host.submittedOperationCount() == beforePointerOps);
  assert(!host.previewActive());
  return 0;
}
