#include "canvas/runtime/runtime_composition.hpp"
#include "canvas/runtime/canvas_runtime_facade.hpp"
#include <cassert>
#include <algorithm>
#include <array>
#include <string>

namespace {
canvas::semantic::ResourceManifestEntry entry(std::uint64_t id) {
  canvas::semantic::ResourceManifestEntry value;
  value.resource_id = canvas::semantic::ResourceId{
      canvas::foundation::ObjectId::fromUint64(id)};
  value.resource_revision = 1U;
  value.content_hash =
      "sha256:4bf5122f344554c53bde2ebb8cd2b7e3d1600ad631c385a5d7cce23c7785459a";
  value.byte_length = 1U;
  value.decode_version = 1U;
  value.metadata = {0x08, 0x01, 0x10, 0x01, 0x18, 0x01, 0x20, 0x01};
  return value;
}
canvas::semantic::ResourceManifest makeManifest(std::uint64_t count) {
  canvas::semantic::ResourceManifest value;
  value.manifest_revision = 1U;
  value.entries.reserve(static_cast<std::size_t>(count));
  for (std::uint64_t id = 1U; id <= count; ++id) value.entries.push_back(entry(id));
  std::sort(value.entries.begin(), value.entries.end(), [](const auto& left, const auto& right) {
    return left.resource_id.value < right.resource_id.value;
  });
  return value;
}

canvas::semantic::ObjectRecord shape(std::uint64_t id) {
  canvas::semantic::ObjectRecord value;
  value.id = canvas::foundation::ObjectId::fromUint64(id);
  value.kind = canvas::semantic::ObjectKind::kShape;
  value.kind_version = 1U;
  value.placement.order_key = canvas::semantic::OrderKey{{static_cast<std::uint8_t>(id)}};
  value.content = canvas::semantic::ShapeContent{1U, 10.0, 10.0};
  return value;
}
}

int main() {
  canvas::runtime::RuntimeComposition composition;
  assert(composition.state() == canvas::runtime::RuntimeCompositionState::kCreated);
  assert(composition.open());
  assert(composition.bindSurface(256U, 256U));
  assert(composition.registerSource(9U));
  // Public Composition input consumes the Composition-owned facade before
  // routing the next pointer batch. Hosts must not attach a second owner.
  auto& controlFacade = composition.controlFacade();
  canvas::runtime::CanvasControlRequest presetRequest{};
  presetRequest.clientId = 0xD001U;
  presetRequest.requestId = 1U;
  presetRequest.target = controlFacade.mountedCanvasTargets().front();
  presetRequest.payload.kind =
      canvas::runtime::CanvasControlPayloadKind::kSelectCreationPreset;
  presetRequest.payload.creationPreset =
      canvas::runtime::CanvasCreationPreset::kEllipse;
  assert(controlFacade.submitCanvasControl(presetRequest).state ==
         canvas::runtime::ProductControlState::kQueued);
  canvas::input::PlatformPointerBatch controlDown;
  controlDown.samples.push_back({9U, 91U, 1U, 1'000U, 20.0F, 20.0F, 0.5F,
                                 0.0F, 0.0F, {}, {},
                                 canvas::input::SampleProvenance::kConfirmedCurrent,
                                 canvas::input::PointerPhase::kDown});
  canvas::input::PlatformPointerBatch controlUp = controlDown;
  controlUp.samples.front().sequence = 2U;
  controlUp.samples.front().timestampNs = 2'000U;
  controlUp.samples.front().x = 80.0F;
  controlUp.samples.front().y = 60.0F;
  controlUp.samples.front().phase = canvas::input::PointerPhase::kUp;
  assert(composition.submit(controlDown));
  assert(composition.submit(controlUp));
  assert(composition.runtime().creationPreset() ==
         canvas::interaction::CreationPreset::kEllipse);
  assert(composition.runtime().semanticObjectCount() == 1U);
  canvas::semantic::ResourceManifest manifest;
  canvas::semantic::ResourceManifestEntry entry;
  entry.resource_id = canvas::semantic::ResourceId{canvas::foundation::ObjectId::fromUint64(7U)};
  entry.resource_revision = 1U;
  entry.content_hash = "sha256:4bf5122f344554c53bde2ebb8cd2b7e3d1600ad631c385a5d7cce23c7785459a";
  entry.byte_length = 1U;
  entry.decode_version = 1U;
  entry.metadata = {0x08, 0x01, 0x10, 0x01, 0x18, 0x01, 0x20, 0x01};
  manifest.entries.push_back(entry);
  manifest.manifest_revision = 1U;
  assert(composition.bindManifest(manifest, 1U));
  auto addedEntry = entry;
  addedEntry.resource_id = canvas::semantic::ResourceId{
      canvas::foundation::ObjectId::fromUint64(8U)};
  canvas::semantic::Operation manifestOperation{
      canvas::semantic::OperationId{canvas::foundation::ObjectId::fromUint64(700U)},
      canvas::semantic::DocumentId{canvas::foundation::ObjectId::fromUint64(1U)},
      1U, 1U,
      canvas::semantic::UpdateResourceManifestOp{
          {addedEntry}, {},
          {{canvas::semantic::ResourceId{canvas::foundation::ObjectId::fromUint64(8U)},
            canvas::semantic::ResourceExpectationState::kNeverUsed, 0U}}, {}, 1U}};
  assert(composition.applyExternalOperation(
      manifestOperation, canvas::semantic::ApplySource::kRemoteSync).disposition ==
      canvas::semantic::ApplyDisposition::kApplied);
  assert(composition.resources().find(canvas::foundation::ObjectId::fromUint64(8U)) != nullptr);
  assert(composition.publishResource(canvas::foundation::ObjectId::fromUint64(7U), {0x01U}));
  assert(composition.ownerPump());
  assert(composition.readback().pendingNotifications == 0U);
  // A validated provider update must cross the composition boundary as a
  // derived refresh/frame demand, rather than stopping at a receipt.
  assert(composition.readback().canonicalDemand);
  assert(composition.resourceReceipt(canvas::foundation::ObjectId::fromUint64(7U)).has_value());
  const auto firstToken = composition.resources().token(canvas::foundation::ObjectId::fromUint64(7U), 0U, 99U);
  assert(firstToken.has_value());
  const auto validated = composition.resourceReceipt(canvas::foundation::ObjectId::fromUint64(7U));
  assert(validated->state == canvas::runtime::ResourceBindingState::kBytesValidated);
  assert(!composition.notifyResource(canvas::foundation::ObjectId::fromUint64(99U)));
  assert(composition.lastResourceError() == canvas::runtime::ResourcePortError::kUnknownResource);
  canvas::semantic::ResourceManifest rebound = manifest;
  rebound.entries.front().resource_id = canvas::semantic::ResourceId{canvas::foundation::ObjectId::fromUint64(8U)};
  rebound.manifest_revision = 2U;
  assert(composition.bindManifest(rebound, 2U));
  assert(composition.readback().lifetimeGeneration == 2U);
  assert(!composition.resourceReceipt(canvas::foundation::ObjectId::fromUint64(7U)).has_value());
  assert(!composition.publishResource(*firstToken, {0x01U}));
  assert(composition.lastResourceError() == canvas::runtime::ResourcePortError::kStaleBinding);

  canvas::runtime::RuntimeComposition queueComposition;
  assert(queueComposition.open());
  assert(queueComposition.bindSurface(256U, 256U));
  assert(queueComposition.bindManifest(makeManifest(256U), 7U));
  assert(queueComposition.publishResource(canvas::foundation::ObjectId::fromUint64(1U), {0x01U}));
  assert(queueComposition.ownerPump());
  const auto queueToken = queueComposition.resources().token(
      canvas::foundation::ObjectId::fromUint64(1U), 0U, 500U);
  assert(queueToken.has_value());
  const auto beforeQueueFull = queueComposition.resourceReceipt(
      canvas::foundation::ObjectId::fromUint64(1U));
  assert(beforeQueueFull.has_value());
  for (std::uint64_t id = 1U; id <= 256U; ++id) {
    assert(queueComposition.notifyResource(canvas::foundation::ObjectId::fromUint64(id)));
  }
  assert(!queueComposition.publishResource(*queueToken, {0x01U}));
  assert(queueComposition.lastResourceError() == canvas::runtime::ResourcePortError::kQueueFull);
  assert(queueComposition.ownerPump());
  const auto afterQueueFull = queueComposition.resourceReceipt(
      canvas::foundation::ObjectId::fromUint64(1U));
  assert(afterQueueFull.has_value());
  assert(afterQueueFull->state == canvas::runtime::ResourceBindingState::kBytesValidated);
  assert(afterQueueFull->providerGeneration == beforeQueueFull->providerGeneration);

  canvas::runtime::RuntimeComposition rebindComposition;
  assert(rebindComposition.open());
  assert(rebindComposition.bindSurface(256U, 256U));
  auto sameResource = makeManifest(1U);
  assert(rebindComposition.bindManifest(sameResource, 11U));
  assert(rebindComposition.publishResource(canvas::foundation::ObjectId::fromUint64(1U), {0x01U}));
  const auto lateToken = rebindComposition.resources().token(
      canvas::foundation::ObjectId::fromUint64(1U), 0U, 100U);
  assert(lateToken.has_value());
  assert(rebindComposition.ownerPump());
  assert(rebindComposition.bindManifest(sameResource, 12U));
  assert(!rebindComposition.publishResource(*lateToken, {0x01U}));
  assert(rebindComposition.lastResourceError() == canvas::runtime::ResourcePortError::kStaleBinding);
  assert(rebindComposition.notifyResource(canvas::foundation::ObjectId::fromUint64(1U)));
  assert(rebindComposition.ownerPump());
  const auto reboundReceipt = rebindComposition.resourceReceipt(
      canvas::foundation::ObjectId::fromUint64(1U));
  assert(reboundReceipt.has_value());
  assert(reboundReceipt->token.documentBinding == 12U);
  assert(reboundReceipt->changed);
  sameResource.entries.clear();
  sameResource.manifest_revision = 2U;
  assert(rebindComposition.bindManifest(sameResource, 13U));
  assert(!rebindComposition.resourceReceipt(canvas::foundation::ObjectId::fromUint64(1U)).has_value());

  // Publishing one resource must not invalidate a token issued for another.
  canvas::runtime::RuntimeComposition independent;
  assert(independent.open());
  assert(independent.bindSurface(256U, 256U));
  auto pairManifest = makeManifest(2U);
  assert(independent.bindManifest(pairManifest, 21U));
  const auto tokenA = independent.resources().token(
      canvas::foundation::ObjectId::fromUint64(1U), 0U, 700U);
  const auto tokenB = independent.resources().token(
      canvas::foundation::ObjectId::fromUint64(2U), 0U, 701U);
  assert(tokenA.has_value() && tokenB.has_value());
  assert(independent.publishResource(*tokenA, {0x01U}));
  assert(independent.publishResource(*tokenB, {0x01U}));

  // Raw Runtime ingress must honor the same Lost fence as Composition::submit.
  canvas::runtime::RuntimeComposition lostRaw;
  assert(lostRaw.open());
  assert(lostRaw.bindSurface(256U, 256U));
  assert(lostRaw.registerSource(17U));
  assert(lostRaw.loseSurface());
  canvas::input::PlatformPointerBatch lostBatch;
  lostBatch.samples.push_back({17U, 1U, 1U, 1'000U, 10.0F, 10.0F, 0.5F, 0.0F, 0.0F,
                               {}, {}, canvas::input::SampleProvenance::kConfirmedCurrent,
                               canvas::input::PointerPhase::kDown});
  assert(!lostRaw.runtime().acceptPlatformBatch(lostBatch, 1'000U));

  assert(composition.loseSurface());
  assert(composition.state() == canvas::runtime::RuntimeCompositionState::kLost);
  assert(!composition.notifyResource(canvas::foundation::ObjectId::fromUint64(7U)));
  assert(composition.rebindSurface());
  assert(composition.state() == canvas::runtime::RuntimeCompositionState::kReady);
  assert(composition.stop());
  assert(composition.state() == canvas::runtime::RuntimeCompositionState::kQuiescent);
  assert(composition.detach());
  assert(composition.close());
  assert(composition.state() == canvas::runtime::RuntimeCompositionState::kClosed);
  assert(!composition.runtime().selectCreationPreset(canvas::interaction::CreationPreset::kRectangle));
  assert(!composition.runtime().undo());
  assert(!composition.runtime().redo());

  // A restore publishes a new control attachment and fences requests queued
  // against the lost surface. Requests using the new target remain usable.
  canvas::runtime::RuntimeComposition restored;
  assert(restored.open());
  assert(restored.bindSurface(256U, 256U));
  auto& restoredFacade = restored.controlFacade();
  const auto oldTarget = restoredFacade.mountedCanvasTargets().front();
  canvas::runtime::CanvasControlRequest oldRequest{};
  oldRequest.clientId = 0xE001U;
  oldRequest.requestId = 1U;
  oldRequest.target = oldTarget;
  oldRequest.payload.kind = canvas::runtime::CanvasControlPayloadKind::kSelectCreationPreset;
  oldRequest.payload.creationPreset = canvas::runtime::CanvasCreationPreset::kEllipse;
  assert(restoredFacade.submitCanvasControl(oldRequest).state ==
         canvas::runtime::ProductControlState::kQueued);
  assert(restored.loseSurface());
  assert(restored.rebindSurface());
  const auto newTarget = restoredFacade.mountedCanvasTargets().front();
  assert(newTarget != oldTarget);
  assert(restored.ownerPump());
  const auto oldReceipt = restoredFacade.canvasControlReceipt({0xE001U, 1U});
  assert(oldReceipt.has_value());
  assert(oldReceipt->state == canvas::runtime::CanvasControlReceiptState::kTargetDestroyed ||
         oldReceipt->state == canvas::runtime::CanvasControlReceiptState::kStaleTarget);
  assert(restored.runtime().creationPreset() !=
         canvas::interaction::CreationPreset::kEllipse);
  canvas::runtime::CanvasControlRequest newRequest = oldRequest;
  newRequest.requestId = 2U;
  newRequest.target = newTarget;
  assert(restoredFacade.submitCanvasControl(newRequest).state ==
         canvas::runtime::ProductControlState::kQueued);
  assert(restored.ownerPump());
  assert(restoredFacade.canvasControlReceipt({0xE001U, 2U})->state ==
         canvas::runtime::CanvasControlReceiptState::kApplied);
  assert(restored.runtime().creationPreset() ==
         canvas::interaction::CreationPreset::kEllipse);

  // Every canonical mutation ingress remains fenced after close, including
  // history compensation and remote replay.  This guards the shared owner
  // boundary instead of only testing UI-facing controls.
  canvas::runtime::RuntimeComposition closedMutation;
  assert(closedMutation.open());
  assert(closedMutation.bindSurface(256U, 256U));
  assert(closedMutation.runtime().selectCreationPreset(
      canvas::interaction::CreationPreset::kRectangle));
  canvas::input::PlatformPointerBatch down;
  down.samples.push_back({1U, 1U, 1U, 1'000U, 10.0F, 10.0F, 0.5F, 0.0F, 0.0F,
                          {}, {}, canvas::input::SampleProvenance::kConfirmedCurrent,
                          canvas::input::PointerPhase::kDown});
  canvas::input::PlatformPointerBatch up = down;
  up.samples.front().sequence = 2U;
  up.samples.front().timestampNs = 2'000U;
  up.samples.front().x = 40.0F;
  up.samples.front().y = 40.0F;
  up.samples.front().phase = canvas::input::PointerPhase::kUp;
  assert(closedMutation.registerSource(1U));
  assert(closedMutation.submit(down));
  assert(closedMutation.submit(up));
  assert(closedMutation.runtime().semanticObjectCount() == 1U);
  assert(closedMutation.runtime().canUndo());
  assert(closedMutation.close());
  const auto beforeClosedRemote = closedMutation.runtime().semanticObjectCount();
  assert(!closedMutation.runtime().undo());
  assert(!closedMutation.runtime().redo());
  canvas::semantic::Operation remote{
      canvas::semantic::OperationId{canvas::foundation::ObjectId::fromUint64(900U)},
      canvas::semantic::DocumentId{canvas::foundation::ObjectId::fromUint64(1U)}, 1U, 1U,
      canvas::semantic::InsertObjectsOp{{shape(901U)}}};
  const auto remoteResult = closedMutation.runtime().applyExternalOperation(
      remote, canvas::semantic::ApplySource::kRemoteSync);
  assert(remoteResult.disposition != canvas::semantic::ApplyDisposition::kApplied);
  assert(closedMutation.runtime().semanticObjectCount() == beforeClosedRemote);

  // Every direct canonical owner ingress must reject writes after the
  // surface is lost, before the semantic engine is touched.
  canvas::runtime::RuntimeComposition lostIngress;
  assert(lostIngress.open());
  assert(lostIngress.bindSurface(256U, 256U));
  assert(lostIngress.registerSource(31U));
  auto& lostRuntime = lostIngress.runtime();
  assert(lostRuntime.selectTool(canvas::interaction::CanvasEditingRuntime::ToolMode::kBrush));
  canvas::input::PlatformPointerBatch brushBatch;
  brushBatch.samples.push_back({31U, 71U, 1U, 1'000U, 10.0F, 10.0F, 0.5F, 0.0F, 0.0F,
                               {}, {}, canvas::input::SampleProvenance::kConfirmedCurrent,
                               canvas::input::PointerPhase::kDown});
  brushBatch.samples.push_back({31U, 71U, 2U, 2'000U, 20.0F, 20.0F, 0.5F, 0.0F, 0.0F,
                               {}, {}, canvas::input::SampleProvenance::kConfirmedCurrent,
                               canvas::input::PointerPhase::kMove});
  assert(lostIngress.submit(brushBatch));
  const auto beforeLostBrushObjects = lostRuntime.semanticObjectCount();
  const auto beforeLostBrushGeneration = lostRuntime.semanticGeneration();
  assert(lostIngress.loseSurface());
  assert(!lostRuntime.finishBrushSession(71U));
  assert(lostRuntime.semanticObjectCount() == beforeLostBrushObjects);
  assert(lostRuntime.semanticGeneration() == beforeLostBrushGeneration);

  canvas::semantic::Operation lostOperation{
      canvas::semantic::OperationId{canvas::foundation::ObjectId::fromUint64(1200U)},
      canvas::semantic::DocumentId{canvas::foundation::ObjectId::fromUint64(1U)}, 1U, 1U,
      canvas::semantic::InsertObjectsOp{{shape(1201U)}}};
  canvas::interaction::OperationRequest lostRequest{1200U, lostOperation};
  auto& operationPort = static_cast<canvas::interaction::OperationSubmitPort&>(lostRuntime);
  assert(!operationPort.submit(lostRequest).accepted);
  assert(lostRuntime.semanticObjectCount() == beforeLostBrushObjects);

  auto& historyPort = static_cast<canvas::interaction::HistorySubmitPort&>(lostRuntime);
  const std::array<canvas::semantic::Operation, 1> historyOperation{lostOperation};
  assert(!historyPort.submit(historyOperation, canvas::semantic::ApplySource::kUndoRedo).accepted);
  assert(lostRuntime.semanticObjectCount() == beforeLostBrushObjects);

  // A successful canonical commit must leave a frame demand for a deferred
  // platform presenter to consume.
  canvas::runtime::RuntimeComposition demandComposition;
  assert(demandComposition.open());
  assert(demandComposition.bindSurface(256U, 256U));
  assert(demandComposition.registerSource(41U));
  demandComposition.runtime().setPlatformPresentationDeferred(true);
  assert(demandComposition.requestCanonicalFrame(
      demandComposition.runtime().nextCanonicalFrameId()));
  assert(!demandComposition.readback().canonicalDemand);
  canvas::input::PlatformPointerBatch demandDown;
  demandDown.samples.push_back({41U, 5U, 1U, 1'000U, 20.0F, 20.0F, 0.5F, 0.0F, 0.0F,
                               {}, {}, canvas::input::SampleProvenance::kConfirmedCurrent,
                               canvas::input::PointerPhase::kDown});
  canvas::input::PlatformPointerBatch demandUp = demandDown;
  demandUp.samples.front().sequence = 2U;
  demandUp.samples.front().timestampNs = 2'000U;
  demandUp.samples.front().x = 80.0F;
  demandUp.samples.front().phase = canvas::input::PointerPhase::kUp;
  assert(demandComposition.submit(demandDown));
  assert(demandComposition.submit(demandUp));
  assert(demandComposition.readback().canonicalDemand);

  // A public frame opportunity must also advance accepted controls and
  // resource notifications when no pointer batch arrives. This is the Web
  // rAF/present path and must not depend on a host-specific ownerPump call.
  canvas::runtime::RuntimeComposition idleFrameComposition;
  assert(idleFrameComposition.open());
  assert(idleFrameComposition.bindSurface(256U, 256U));
  auto idleManifest = makeManifest(1U);
  assert(idleFrameComposition.bindManifest(idleManifest, 31U));
  auto& idleFacade = idleFrameComposition.controlFacade();
  assert(idleFrameComposition.publishResource(
      canvas::foundation::ObjectId::fromUint64(1U), {0x01U}));
  const auto idleControl = idleFacade.setSelectionMode(true, 1U, 1U);
  assert(idleControl.state == canvas::runtime::ProductControlState::kQueued);
  assert(!idleFrameComposition.runtime().selectionMode());
  assert(idleFrameComposition.requestCanonicalFrame(
      idleFrameComposition.runtime().nextCanonicalFrameId()));
  const auto idleReceipt = idleFacade.canvasControlReceipt({1U, 1U});
  assert(idleReceipt.has_value());
  assert(idleReceipt->state ==
         canvas::runtime::CanvasControlReceiptState::kApplied);
  assert(idleFrameComposition.runtime().selectionMode());
  assert(idleFrameComposition.readback().pendingNotifications == 0U);
  assert(idleFrameComposition.resourceReceipt(
      canvas::foundation::ObjectId::fromUint64(1U)).has_value());

  // A resource notification accepted immediately before a public frame
  // opportunity must be consumed by that same opportunity; hosts must not
  // need a second timer tick to discover the canonical demand.
  canvas::runtime::RuntimeComposition sameOpportunity;
  assert(sameOpportunity.open());
  assert(sameOpportunity.bindSurface(256U, 256U));
  assert(sameOpportunity.bindManifest(idleManifest, 32U));
  sameOpportunity.runtime().setPlatformPresentationDeferred(true);
  canvas::runtime::RuntimeFrameOpportunity firstOpportunity{};
  assert(sameOpportunity.serviceFrameOpportunity(true, firstOpportunity));
  assert(firstOpportunity.canonicalRequested);
  assert(firstOpportunity.canonicalFrame.has_value());
  const auto firstIdentity = *firstOpportunity.canonicalFrame;
  assert(sameOpportunity.receivePlatformPresented({
      firstIdentity.viewId, firstIdentity.frameId, firstIdentity.surfaceGeneration,
      firstIdentity.metricsGeneration, canvas::render::PresentOutcome::kPresented,
      canvas::render::PresentEvidenceKind::kPlatformQualified, std::nullopt}));
  assert(sameOpportunity.publishResource(
      canvas::foundation::ObjectId::fromUint64(1U), {0x01U}));
  canvas::runtime::RuntimeFrameOpportunity resourceOpportunity{};
  assert(sameOpportunity.serviceFrameOpportunity(false, resourceOpportunity));
  assert(resourceOpportunity.canonicalRequested);
  assert(resourceOpportunity.needsPaint);
  assert(sameOpportunity.readback().pendingNotifications == 0U);

  // Automatic and deferred presentation modes must share the same public
  // frame contract. An automatic control application must not consume the
  // frame id that the surrounding frame opportunity is about to submit.
  for (const bool deferred : {false, true}) {
    canvas::runtime::RuntimeComposition cameraComposition;
    assert(cameraComposition.open());
    assert(cameraComposition.bindSurface(256U, 256U));
    cameraComposition.runtime().setPlatformPresentationDeferred(deferred);
    auto& cameraFacade = cameraComposition.controlFacade();
    const auto queued = cameraFacade.zoomAt(20.0F, 20.0F, 2.0F, 1U, 1U);
    assert(queued.state == canvas::runtime::ProductControlState::kQueued);
    const auto requestedFrame = cameraComposition.runtime().nextCanonicalFrameId();
    assert(cameraComposition.requestCanonicalFrame(requestedFrame));
    const auto terminal = cameraFacade.canvasControlReceipt({1U, 1U});
    assert(terminal.has_value());
    assert(terminal->state ==
           canvas::runtime::CanvasControlReceiptState::kApplied);
    assert(cameraComposition.runtime().viewportGesture().scale == 2.0F);
    const auto submitted = cameraComposition.runtime().lastSubmittedCanonicalFrame();
    assert(submitted.has_value());
    assert(submitted->frameId.value() == requestedFrame);
    assert(cameraComposition.runtime().platformPresentationDeferred() == deferred);
  }

  // A superseded preview capture must not pin the composition to stale
  // pixels after a newer move arrives before the platform acknowledgement.
  canvas::runtime::RuntimeComposition supersededPreview;
  assert(supersededPreview.open());
  assert(supersededPreview.bindSurface(256U, 256U));
  assert(supersededPreview.registerSource(51U));
  assert(supersededPreview.setPlatformPresentationDeferred(true));
  canvas::input::PlatformPointerBatch previewDown;
  previewDown.samples.push_back({51U, 501U, 1U, 1'000U, 20.0F, 20.0F, 0.5F,
                                 0.0F, 0.0F, {}, {},
                                 canvas::input::SampleProvenance::kConfirmedCurrent,
                                 canvas::input::PointerPhase::kDown});
  canvas::input::PlatformPointerBatch previewMove = previewDown;
  previewMove.samples.front().sequence = 2U;
  previewMove.samples.front().timestampNs = 2'000U;
  previewMove.samples.front().x = 80.0F;
  previewMove.samples.front().y = 80.0F;
  previewMove.samples.front().phase = canvas::input::PointerPhase::kMove;
  assert(supersededPreview.submit(previewDown));
  assert(supersededPreview.submit(previewMove));
  canvas::runtime::RuntimeFrameOpportunity firstPreview{};
  assert(supersededPreview.serviceFrameOpportunity(false, firstPreview));
  assert(firstPreview.previewRendered);
  const auto firstPreviewRevision = firstPreview.previewRevision;
  std::array<std::uint8_t, 256U * 256U * 4U> firstPreviewPixels{};
  assert(supersededPreview.readPreviewRgba(firstPreviewPixels));
  previewMove.samples.front().sequence = 3U;
  previewMove.samples.front().timestampNs = 3'000U;
  previewMove.samples.front().x = 180.0F;
  previewMove.samples.front().y = 180.0F;
  assert(supersededPreview.submit(previewMove));
  assert(!supersededPreview.finishFrameOpportunity({firstPreview.previewWork, false}));
  canvas::runtime::RuntimeFrameOpportunity secondPreview{};
  assert(supersededPreview.serviceFrameOpportunity(false, secondPreview));
  assert(secondPreview.previewRendered);
  assert(secondPreview.previewRevision > firstPreviewRevision);
  std::array<std::uint8_t, 256U * 256U * 4U> secondPreviewPixels{};
  assert(supersededPreview.readPreviewRgba(secondPreviewPixels));
  assert(firstPreviewPixels != secondPreviewPixels);
  assert(supersededPreview.finishFrameOpportunity({secondPreview.previewWork, true}));

  // A completion for superseded work A must not qualify replacement work B.
  canvas::runtime::RuntimeComposition delayedPreview;
  assert(delayedPreview.open());
  assert(delayedPreview.bindSurface(256U, 256U));
  assert(delayedPreview.registerSource(52U));
  assert(delayedPreview.setPlatformPresentationDeferred(true));
  auto delayedDown = previewDown;
  delayedDown.samples.front().source = 52U;
  delayedDown.samples.front().pointer = 502U;
  auto delayedMove = delayedDown;
  delayedMove.samples.front().sequence = 2U;
  delayedMove.samples.front().timestampNs = 2'000U;
  delayedMove.samples.front().phase = canvas::input::PointerPhase::kMove;
  delayedMove.samples.front().x = 80.0F;
  delayedMove.samples.front().y = 80.0F;
  assert(delayedPreview.submit(delayedDown));
  assert(delayedPreview.submit(delayedMove));
  canvas::runtime::RuntimeFrameOpportunity delayedA{};
  assert(delayedPreview.serviceFrameOpportunity(false, delayedA));
  assert(delayedA.previewWork.has_value());
  auto delayedBMove = delayedMove;
  delayedBMove.samples.front().sequence = 3U;
  delayedBMove.samples.front().timestampNs = 3'000U;
  delayedBMove.samples.front().x = 180.0F;
  delayedBMove.samples.front().y = 180.0F;
  assert(delayedPreview.submit(delayedBMove));
  canvas::runtime::RuntimeFrameOpportunity delayedB{};
  assert(delayedPreview.serviceFrameOpportunity(false, delayedB));
  assert(delayedB.previewWork.has_value() && delayedB.previewWork != delayedA.previewWork);
  const auto qualifiedBefore = delayedPreview.previewRenderState().platformQualifiedRevision;
  assert(!delayedPreview.finishFrameOpportunity({delayedA.previewWork, true}));
  assert(delayedPreview.previewRenderState().platformQualifiedRevision == qualifiedBefore);
  assert(delayedPreview.previewRenderState().dirty);
  assert(delayedPreview.finishFrameOpportunity({delayedB.previewWork, true}));
  assert(!delayedPreview.previewRenderState().dirty);
  return 0;
}
