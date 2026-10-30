#include "../common/canvas_control_binding.hpp"

#include <cassert>
#include <cmath>
#include <limits>

using namespace canvas::ink_playground;
using namespace canvas::runtime;

int main() {
  InkPlaygroundHost host;
  assert(host.bindSurface(256, 256));
  CanvasControlBinding binding(host, {1, 1, 1, 1});

  CanvasControlRequest options{};
  options.clientId = 1;
  options.requestId = 1;
  options.target = {1, 1, 1, 1};
  options.payload.kind = CanvasControlPayloadKind::kInkOptions;
  options.payload.ink.size = OptionPatch<float>::set(4.0F);
  options.payload.ink.color = OptionPatch<RgbaColor>::set({1.0F, 0.0F, 0.0F, 1.0F});
  options.payload.ink.opacity = OptionPatch<float>::set(0.25F);
  assert(binding.apply(options).error == CanvasControlError::kNone);
  assert(binding.snapshot({1, 1, 1, 1}).size == 4.0F);
  assert(binding.snapshot({1, 1, 1, 1}).color.r == 1.0F);
  assert(binding.snapshot({1, 1, 1, 1}).opacity == 0.25F);

  assert(host.beginBrushSession(7, 1));
  assert(!host.setInkOptions(12.0F, 0.0F, 0.0F, 1.0F, 1.0F, 1.0F));
  assert(host.appendBrushSample(7, 10.0, 10.0, 0.5, 1));
  assert(host.finishBrushSession(7));
  assert(host.semanticObjectCount() == 1U);

  CanvasControlRequest invalid = options;
  invalid.requestId = 2;
  invalid.payload.ink.size = OptionPatch<float>::set(std::numeric_limits<float>::quiet_NaN());
  const auto invalidResult = binding.apply(invalid);
  assert(invalidResult.error == CanvasControlError::kBusy ||
         invalidResult.error == CanvasControlError::kInvalidValue);

  CanvasControlRequest eraser{};
  eraser.clientId = 1; eraser.requestId = 3; eraser.target = {1, 1, 1, 1};
  eraser.payload.kind = CanvasControlPayloadKind::kEraserOptions;
  eraser.payload.eraser.diameterLogicalPx = OptionPatch<float>::set(20.0F);
  assert(binding.apply(eraser).error == CanvasControlError::kNone);
  assert(host.eraserDiameterLogicalPx() == 20.0F);
  CanvasControlRequest patch = options;
  patch.requestId = 4;
  patch.payload.ink = {};
  patch.payload.ink.opacity = OptionPatch<float>::set(0.0F);
  assert(binding.apply(patch).error == CanvasControlError::kNone);
  assert(host.inkOpacity() == 0.0F);
  patch.requestId = 5;
  patch.payload.ink.opacity = OptionPatch<float>::clear();
  assert(binding.apply(patch).error == CanvasControlError::kNone);
  assert(host.inkOpacity() == 1.0F);
  CanvasControlRequest preset = options;
  preset.requestId = 6;
  preset.payload.kind = CanvasControlPayloadKind::kSelectPreset;
  preset.payload.preset = {"marker-flat-v1", 1};
  assert(binding.apply(preset).error == CanvasControlError::kNone);
  // An opacity override was cleared, so Marker must expose its 0.7 default.
  // The explicit red color/size overrides remain intact.
  assert(std::abs(host.inkOpacity() - 0.7F) < 0.0001F);
  assert(host.inkSize() == 4.0F && host.inkColor()[0] == 1.0F);
  return 0;
}
