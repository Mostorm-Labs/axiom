#include "../common/canvas_control_binding.hpp"
#include "canvas/ink/brush_session.hpp"
#include "canvas/ink/brush_package_catalog.hpp"
#include "canvas/ink/resolved_brush_state.hpp"

#include <cassert>
#include <cmath>

using namespace canvas::ink_playground;
using namespace canvas::runtime;
using namespace canvas::ink;

int main() {
  InkPlaygroundHost host;
  assert(host.bindSurface(320, 240));
  CanvasControlBinding binding(host, {1, 1, 1, 1});

  CanvasControlRequest pan{};
  pan.clientId = 1; pan.requestId = 1; pan.target = {1, 1, 1, 1};
  pan.payload.kind = CanvasControlPayloadKind::kSelectTool;
  pan.payload.tool = CanvasToolKind::kPan;
  assert(binding.apply(pan).error == CanvasControlError::kNone);
  assert(binding.snapshot(pan.target).tool == CanvasToolKind::kPan);

  CanvasControlRequest eraser{};
  eraser.clientId = 1; eraser.requestId = 2; eraser.target = pan.target;
  eraser.payload.kind = CanvasControlPayloadKind::kEraserOptions;
  eraser.payload.eraser.mode = OptionPatch<std::uint32_t>::set(1U);
  eraser.payload.eraser.diameterLogicalPx = OptionPatch<float>::set(24.0F);
  assert(binding.apply(eraser).error == CanvasControlError::kNone);
  const auto eraseSnapshot = binding.snapshot(pan.target);
  assert(eraseSnapshot.eraserMode == 1U);
  assert(std::abs(eraseSnapshot.eraserDiameterLogicalPx - 24.0F) < 0.0001F);
  assert(eraseSnapshot.targetAvailable && eraseSnapshot.viewportWidth == 320.0F);

  CanvasControlRequest presets = pan;
  presets.requestId = 3;
  presets.payload.kind = CanvasControlPayloadKind::kSelectPreset;
  presets.payload.preset = {"vector-solid-v1", 1};
  assert(binding.apply(presets).error == CanvasControlError::kNone);

  BrushPackageCatalog catalog;
  const auto loaded = catalog.loadDefault("vector-solid-v1", 1);
  assert(loaded);
  auto resolved = resolveBrushState(loaded.package, 7);
  resolved.package.paint.red = 1.0;
  resolved.package.paint.green = 0.0;
  resolved.package.paint.blue = 0.0;
  resolved.package.paint.opacity = 0.25;
  BrushSession session(7, resolved);
  assert(session.begin());
  BrushPreviewDelta delta;
  BrushSample sample{10.0, 10.0, 0.5, true, 1};
  assert(session.append(std::span<const BrushSample>(&sample, 1), {}, delta) == BrushSessionError::kNone);
  assert(delta.hasPaint);
  assert(std::abs(delta.red - 1.0F) < 0.0001F);
  assert(std::abs(delta.opacity - 0.25F) < 0.0001F);
  return 0;
}
