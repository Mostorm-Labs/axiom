#include "../common/canvas_control_binding.hpp"
#include "canvas/control/canvas_control_service.hpp"

#include <cassert>

using namespace canvas::ink_playground;
using namespace canvas::runtime;

int main() {
  InkPlaygroundHost host;
  assert(host.bindSurface(320, 240));
  CanvasControlBinding binding(host, {1, 1, 1, 1});
  CanvasControlService service(binding, 8, 8);

  CanvasControlRequest tool{};
  tool.clientId = 11; tool.requestId = 1; tool.target = {1, 1, 1, 1};
  tool.payload.kind = CanvasControlPayloadKind::kSelectTool;
  tool.payload.tool = CanvasToolKind::kSelect;
  assert(service.enqueue(tool).state == CanvasControlReceiptState::kQueued);
  assert(service.processPending(1) == 1);
  assert(service.receipt({11, 1})->state == CanvasControlReceiptState::kApplied);
  assert(binding.snapshot(tool.target).tool == CanvasToolKind::kSelect);

  CanvasControlRequest stale = tool;
  stale.requestId = 2; stale.target.viewEpoch = 2;
  assert(service.enqueue(stale).state == CanvasControlReceiptState::kStaleTarget);
  assert(binding.snapshot(tool.target).tool == CanvasToolKind::kSelect);

  CanvasControlRequest camera = tool;
  camera.requestId = 3; camera.payload.kind = CanvasControlPayloadKind::kSetZoom;
  camera.payload.scale = 2.0F; camera.payload.hasAnchor = false;
  assert(service.enqueue(camera).state == CanvasControlReceiptState::kQueued);
  assert(service.processPending(2) == 1);
  assert(service.receipt({11, 3})->state == CanvasControlReceiptState::kApplied);
  assert(host.viewportGesture().scale == 2.0F);
  return 0;
}
