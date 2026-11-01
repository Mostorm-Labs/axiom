#include "canvas/runtime/canvas_runtime_facade.hpp"

#include <cstdio>

namespace {
canvas::input::PlatformPointerBatch pointer(std::uint64_t sequence,
                                             canvas::input::PointerPhase phase,
                                             float x, float y) {
  canvas::input::PlatformPointerBatch batch;
  batch.samples.push_back({1U, 7U, sequence, sequence * 1000U, x, y, 0.5F,
                           0.0F, 0.0F, {}, {},
                           canvas::input::SampleProvenance::kConfirmedCurrent,
                           phase});
  return batch;
}
}

#define CHECK(condition) do { \
  if (!(condition)) { \
    std::fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #condition); \
    return 1; \
  } \
} while (0)

int main() {
  using namespace canvas;
  interaction::CanvasEditingRuntime runtime;
  CHECK(runtime.bindSurface(256U, 256U));
  CHECK(runtime.selectCreationPreset(interaction::CreationPreset::kRectangle));
  CHECK(runtime.acceptPlatformBatch(pointer(1U, input::PointerPhase::kDown, 20, 20), 1000U));
  CHECK(runtime.acceptPlatformBatch(pointer(2U, input::PointerPhase::kUp, 100, 80), 2000U));
  CHECK(runtime.setSelectionMode(true));
  CHECK(runtime.selectAtViewPoint(50, 45));
  CHECK(runtime.setSelectedAppearance({true, {0.9F, 0.1F, 0.2F, 1.0F}, true,
                                       {0.2F, 0.3F, 0.4F, 1.0F}, 5.0F, 0.25F}));
  runtime::CanvasRuntimeFacade facade(runtime, {1U, 1U, 1U, 1U});
  const auto original = facade.readCanvasControlSnapshot(facade.target());
  runtime::CanvasControlRequest request{};
  request.clientId = 4U;
  request.requestId = 1U;
  request.target = facade.target();
  request.payload.kind = runtime::CanvasControlPayloadKind::kSelectedAppearance;
  request.payload.appearance = original.selectedAppearance;
  request.payload.appearance.opacity = 0.6F;
  CHECK(facade.submitCanvasControl(request).state == runtime::ProductControlState::kQueued);
  CHECK(runtime.setSelectedStrokeWidth(19.0));
  const auto concurrent = facade.readCanvasControlSnapshot(facade.target());
  CHECK(concurrent.semanticGeneration != original.semanticGeneration);
  CHECK(facade.process(1U) == 0U);
  const auto stale = facade.canvasControlReceipt({4U, 1U});
  CHECK(stale.has_value());
  CHECK(stale->state == runtime::CanvasControlReceiptState::kStaleTarget);
  CHECK(stale->error == runtime::CanvasControlError::kStaleTarget);
  const auto rejected = facade.readCanvasControlSnapshot(facade.target());
  CHECK(rejected.semanticGeneration == concurrent.semanticGeneration);
  CHECK(rejected.selectedAppearance.opacity == 0.25F);
  CHECK(rejected.selectedAppearance.strokeWidth == 19.0F);

  request.requestId = 2U;
  request.payload.appearance = rejected.selectedAppearance;
  request.payload.appearance.opacity = 0.6F;
  CHECK(facade.submitCanvasControl(request).state == runtime::ProductControlState::kQueued);
  CHECK(facade.process(2U) == 1U);
  const auto applied = facade.canvasControlReceipt({4U, 2U});
  CHECK(applied.has_value());
  CHECK(applied->state == runtime::CanvasControlReceiptState::kApplied);
  const auto after = facade.readCanvasControlSnapshot(facade.target());
  CHECK(after.selectedAppearance.opacity == 0.6F);
  CHECK(after.selectedAppearance.strokeWidth == 19.0F);
  CHECK(after.selectedAppearance.fill == original.selectedAppearance.fill);
  std::puts("PASS stale receipt and explicit retry preserve concurrent stroke width");
  return 0;
}
