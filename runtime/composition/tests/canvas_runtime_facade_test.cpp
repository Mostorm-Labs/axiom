#include "canvas/runtime/canvas_runtime_facade.hpp"

#include <cassert>

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

int main() {
  canvas::interaction::CanvasEditingRuntime runtime;
  canvas::runtime::CanvasRuntimeFacade facade(runtime, {1U, 1U, 1U, 1U});

  canvas::runtime::StickyCreateRequest stickyRequest{};
  stickyRequest.objectId = canvas::foundation::ObjectId::fromUint64(700U);
  const auto stickyReceipt = facade.createSticky(stickyRequest);
  assert(stickyReceipt.state == canvas::runtime::ConnectorCommandState::kRejected);

  const auto brush = facade.setBrush(2U, 1U, 1U, 1U);
  assert(brush.state == canvas::runtime::ProductControlState::kQueued);
  assert(runtime.selectedBrushProfile() != "marker-flat-v1");
  assert(facade.process(1U) == 1U);
  assert(runtime.selectedBrushProfile() == "marker-flat-v1");
  assert(facade.controlSequence() == 1U);
  const auto brushReceipt = facade.canvasControlReceipt({1U, 1U});
  assert(brushReceipt.has_value());
  assert(brushReceipt->state == canvas::runtime::CanvasControlReceiptState::kApplied);

  const auto creation = facade.submitCanvasControl([&] {
    canvas::runtime::CanvasControlRequest request{};
    request.clientId = 1U;
    request.requestId = 2U;
    request.target = facade.target();
    request.payload.kind = canvas::runtime::CanvasControlPayloadKind::kSelectCreationPreset;
    request.payload.creationPreset = canvas::runtime::CanvasCreationPreset::kEllipse;
    return request;
  }());
  assert(creation.state == canvas::runtime::ProductControlState::kQueued);
  assert(facade.process(2U) == 1U);
  assert(runtime.creationPreset() == canvas::interaction::CreationPreset::kEllipse);

  const auto closedBefore = runtime.inkSize();
  runtime.close();
  canvas::runtime::CanvasControlRequest closedRequest{};
  closedRequest.clientId = 2U;
  closedRequest.requestId = 1U;
  closedRequest.target = facade.target();
  closedRequest.payload.kind = canvas::runtime::CanvasControlPayloadKind::kInkOptions;
  closedRequest.payload.ink.size = canvas::runtime::OptionPatch<float>::set(42.0F);
  const auto closed = facade.submitCanvasControl(closedRequest);
  assert(closed.state == canvas::runtime::ProductControlState::kRejected);
  assert(runtime.inkSize() == closedBefore);
  assert(facade.mountedCanvasTargets().empty());

  canvas::interaction::CanvasEditingRuntime orderedRuntime;
  canvas::runtime::CanvasRuntimeFacade orderedFacade(orderedRuntime, {1U, 1U, 1U, 1U});
  auto select = canvas::runtime::CanvasControlRequest{};
  select.clientId = 3U; select.requestId = 1U; select.target = orderedFacade.target();
  select.payload.kind = canvas::runtime::CanvasControlPayloadKind::kSelectTool;
  select.payload.tool = canvas::runtime::CanvasToolKind::kSelect;
  assert(orderedFacade.submitCanvasControl(select).state == canvas::runtime::ProductControlState::kQueued);
  assert(orderedFacade.process(1U) == 1U);
  assert(orderedRuntime.selectionMode());

  canvas::interaction::CanvasEditingRuntime styleRuntime;
  assert(styleRuntime.bindSurface(256U, 256U));
  assert(styleRuntime.selectCreationPreset(canvas::interaction::CreationPreset::kRectangle));
  assert(styleRuntime.acceptPlatformBatch(pointer(1U, canvas::input::PointerPhase::kDown, 20, 20), 1000U));
  assert(styleRuntime.acceptPlatformBatch(pointer(2U, canvas::input::PointerPhase::kUp, 100, 80), 2000U));
  assert(styleRuntime.setSelectionMode(true));
  assert(styleRuntime.selectAtViewPoint(50, 45));
  assert(styleRuntime.setSelectedAppearance({true, {0.9F, 0.1F, 0.2F, 1.0F}, true,
                                             {0.2F, 0.3F, 0.4F, 1.0F}, 5.0F, 0.25F}));
  assert(styleRuntime.setSelectedTransform({1, 0, 0, 1, 17, 0}));
  canvas::runtime::CanvasRuntimeFacade styleFacade(styleRuntime, {1U, 1U, 1U, 1U});
  const auto before = styleFacade.readCanvasControlSnapshot(styleFacade.target());
  assert(before.selectedTransform[4] == 17.0);
  assert(before.selectedAppearance.fill.r == 0.9F);
  assert(before.selectedAppearance.opacity == 0.25F);
  auto opacity = canvas::runtime::CanvasControlRequest{};
  opacity.clientId = 4U; opacity.requestId = 1U; opacity.target = styleFacade.target();
  opacity.payload.kind = canvas::runtime::CanvasControlPayloadKind::kSelectedAppearance;
  opacity.payload.appearance = before.selectedAppearance;
  opacity.payload.appearance.opacity = 0.6F;
  assert(styleFacade.submitCanvasControl(opacity).state == canvas::runtime::ProductControlState::kQueued);
  assert(styleRuntime.setSelectedStrokeWidth(19.0));
  const auto concurrent = styleFacade.readCanvasControlSnapshot(styleFacade.target());
  assert(concurrent.semanticGeneration != before.semanticGeneration);
  assert(styleFacade.process(1U) == 0U);
  const auto staleStyle = styleFacade.canvasControlReceipt({4U, 1U});
  assert(staleStyle.has_value());
  assert(staleStyle->state == canvas::runtime::CanvasControlReceiptState::kStaleTarget);
  assert(staleStyle->error == canvas::runtime::CanvasControlError::kStaleTarget);
  const auto afterRejection = styleFacade.readCanvasControlSnapshot(styleFacade.target());
  assert(afterRejection.semanticGeneration == concurrent.semanticGeneration);
  assert(afterRejection.selectedAppearance.opacity == 0.25F);
  assert(afterRejection.selectedAppearance.strokeWidth == 19.0F);

  // Explicitly refresh the field patch and use a fresh request identity.
  opacity.requestId = 2U;
  opacity.payload.appearance = afterRejection.selectedAppearance;
  opacity.payload.appearance.opacity = 0.6F;
  assert(styleFacade.submitCanvasControl(opacity).state == canvas::runtime::ProductControlState::kQueued);
  assert(styleFacade.process(2U) == 1U);
  const auto refreshedStyle = styleFacade.canvasControlReceipt({4U, 2U});
  assert(refreshedStyle.has_value());
  assert(refreshedStyle->state == canvas::runtime::CanvasControlReceiptState::kApplied);
  const auto styleSnapshot = styleFacade.readCanvasControlSnapshot(styleFacade.target());
  assert(styleSnapshot.selectedAppearance.fill.r == 0.9F);
  assert(styleSnapshot.selectedTransform[4] == 17.0);
  assert(styleSnapshot.selectedAppearance.opacity == 0.6F);
  assert(styleSnapshot.selectedAppearance.strokeWidth == 19.0F);

  canvas::interaction::CanvasEditingRuntime legacyRuntime;
  canvas::runtime::CanvasRuntimeFacade legacyFacade(legacyRuntime, {1U, 1U, 1U, 1U});
  canvas::runtime::ProductControlRequest legacy{};
  legacy.requestId = 1U; legacy.runtimeGeneration = 1U;
  legacy.action = canvas::runtime::ProductControlAction::kSetEraser;
  legacy.eraserId = 1U;
  assert(legacyFacade.submitProductControl(legacy).state == canvas::runtime::ProductControlState::kQueued);
  assert(legacyFacade.process(1U) == 1U);
  assert(legacyRuntime.toolMode() == canvas::interaction::CanvasEditingRuntime::ToolMode::kObjectEraser);
  legacy.requestId = 2U; legacy.action = canvas::runtime::ProductControlAction::kSetBrush;
  legacy.brushId = 1U; legacy.brushRevision = 1U;
  assert(legacyFacade.submitProductControl(legacy).state == canvas::runtime::ProductControlState::kQueued);
  assert(legacyFacade.process(2U) == 1U);
  assert(legacyRuntime.toolMode() == canvas::interaction::CanvasEditingRuntime::ToolMode::kBrush);

  facade.setTarget({1U, 1U, 1U, 2U});
  auto staleRequest = canvas::runtime::CanvasControlRequest{};
  staleRequest.clientId = 1U;
  staleRequest.requestId = 3U;
  staleRequest.target = {1U, 1U, 1U, 1U};
  staleRequest.payload.kind = canvas::runtime::CanvasControlPayloadKind::kSelectTool;
  staleRequest.payload.tool = canvas::runtime::CanvasToolKind::kSelect;
  const auto stale = facade.submitCanvasControl(staleRequest);
  assert(stale.state == canvas::runtime::ProductControlState::kRejected);
  assert(stale.control.has_value());
  assert(stale.control->state == canvas::runtime::CanvasControlReceiptState::kStaleTarget);

  // Full-ID readback must distinguish objects with identical low 64 bits.
  canvas::interaction::CanvasEditingRuntime identityRuntime;
  assert(identityRuntime.bindSurface(256U, 256U));
  canvas::runtime::CanvasRuntimeFacade identityFacade(identityRuntime, {7U, 2U, 3U, 4U});
  canvas::semantic::ObjectRecord first{};
  first.id = canvas::foundation::ObjectId::fromUint64(9U);
  first.id.bytes[0] = 0xA5U;
  first.kind = canvas::semantic::ObjectKind::kShape;
  first.kind_version = 1U;
  first.placement.order_key = canvas::semantic::OrderKey{{0x40U}};
  first.content = canvas::semantic::ShapeContent{1U, 40.0, 40.0};
  auto second = first;
  second.id.bytes[0] = 0x5AU;
  second.placement.order_key = canvas::semantic::OrderKey{{0x80U}};
  second.transform.tx = 100.0;
  const canvas::semantic::Operation seed{
      canvas::semantic::OperationId{canvas::foundation::ObjectId::fromUint64(500U)},
      canvas::semantic::DocumentId{canvas::foundation::ObjectId::fromUint64(1U)},
      1U, 1U, canvas::semantic::InsertObjectsOp{{first, second}}};
  assert(identityRuntime.applyExternalOperation(seed, canvas::semantic::ApplySource::kRemoteSync).disposition ==
         canvas::semantic::ApplyDisposition::kApplied);
  assert(identityRuntime.setSelectionMode(true));
  assert(identityRuntime.selectAtViewPoint(20, 20));
  const auto firstReadback = identityFacade.readCanvasControlSnapshot(identityFacade.target());
  assert(firstReadback.selectedObject == first.id);
  assert(identityRuntime.selectAtViewPoint(120, 20));
  const auto secondReadback = identityFacade.readCanvasControlSnapshot(identityFacade.target());
  assert(secondReadback.selectedObject == second.id);
  assert(firstReadback.selectedObject != secondReadback.selectedObject);
  return 0;
}
