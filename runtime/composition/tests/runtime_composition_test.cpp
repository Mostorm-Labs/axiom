#include "canvas/runtime/runtime_composition.hpp"
#include "canvas/runtime/canvas_runtime_facade.hpp"
#include <cassert>
#include <algorithm>
#include <array>
#include <string>
#include <functional>
#include <limits>
#include <thread>

namespace {
class ResizeFailureProvider final : public canvas::render::SkiaSurfaceProvider {
 public:
  canvas::render::RasterSkiaSurfaceProvider actual;
  bool failResize = false;
  bool destructiveFailure = false;
  std::function<void()> duringResize;
  canvas::render::RenderTargetInfo describe() const noexcept override { return actual.describe(); }
  canvas::render::SkiaSurfaceAcquireResult acquire() noexcept override { return actual.acquire(); }
  void release() noexcept override { actual.release(); }
  canvas::render::BackendSubmissionResult lose() noexcept override { return actual.lose(); }
  canvas::render::BackendSubmissionResult present() noexcept override { return actual.present(); }
  canvas::render::BackendSubmissionResult resize(std::uint32_t w, std::uint32_t h) noexcept override {
    if (duringResize) { auto callback = std::move(duringResize); callback(); }
    if (failResize && destructiveFailure) (void)actual.lose();
    return failResize ? canvas::render::BackendSubmissionResult::rejected("injected resize failure")
                      : actual.resize(w, h);
  }
  canvas::render::BackendSubmissionResult readbackRgba(std::span<std::uint8_t> out) noexcept override { return actual.readbackRgba(out); }
  std::uint64_t generation() const noexcept override { return actual.generation(); }
  canvas::render::BackendSubmissionResult advanceGeneration() noexcept override { return actual.advanceGeneration(); }
  std::uint64_t readbackCount() const noexcept override { return actual.readbackCount(); }
  std::uint64_t cpuCopyCount() const noexcept override { return actual.cpuCopyCount(); }
  std::uint64_t presentCount() const noexcept override { return actual.presentCount(); }
};

void stagedProvidersAndDualRoleFailure() {
  canvas::runtime::RuntimeComposition composition;
  assert(composition.open() && composition.bindSurface(256U, 256U));
  const auto before = composition.surfaceSnapshot();
  const auto oldTarget = composition.controlFacade().mountedCanvasTargets().front();
  auto canonical = std::make_unique<canvas::render::RasterSkiaSurfaceProvider>();
  assert(canonical->resize(256U, 256U).code == canvas::render::BackendSubmissionCode::kAccepted);
  assert(composition.registerSurfaceProvider("candidate-canonical", std::move(canonical)));
  // Candidate registration must retain the whole accepted binding and target.
  assert(composition.surfaceSnapshot().surfaceGeneration == before.surfaceGeneration);
  assert(composition.controlFacade().mountedCanvasTargets().front() == oldTarget);
  auto preview = std::make_unique<ResizeFailureProvider>();
  auto* previewRaw = preview.get();
  assert(preview->resize(256U, 256U).code == canvas::render::BackendSubmissionCode::kAccepted);
  assert(composition.registerPreviewSurfaceProvider("candidate-preview", std::move(preview)));
  assert(composition.resizeSurface(256U, 256U));
  assert(composition.controlFacade().mountedCanvasTargets().front() != oldTarget);
  previewRaw->failResize = true;
  previewRaw->destructiveFailure = true;
  assert(!composition.resizeSurface(320U, 240U));
  assert(composition.state() == canvas::runtime::RuntimeCompositionState::kLost);
  assert(!composition.surfaceSnapshot().available);
  assert(!composition.qualificationRuntime().activeSurfaceProvider()->acquire().frame.surface);
  assert(!composition.qualificationRuntime().previewSurfaceProvider()->acquire().frame.surface);
  previewRaw->failResize = false;
  assert(composition.rebindSurface(canvas::render::SurfaceMetrics{320.0F, 240.0F, 320U, 240U, 1.0F, 1.0F}));
  assert(composition.surfaceSnapshot().available);
}

void failedCandidatesKeepAcceptedBindingAndReentrantCloseAborts() {
  using namespace canvas;
  runtime::RuntimeComposition composition;
  assert(composition.open() && composition.bindSurface(256U, 256U));
  const auto before = composition.surfaceSnapshot();
  const auto target = composition.controlFacade().mountedCanvasTargets().front();
  auto failed = std::make_unique<ResizeFailureProvider>();
  assert(failed->resize(256U, 256U).code == render::BackendSubmissionCode::kAccepted);
  failed->failResize = true;
  assert(composition.registerSurfaceProvider("failed-candidate", std::move(failed)));
  auto preview = std::make_unique<render::RasterSkiaSurfaceProvider>();
  assert(preview->resize(320U, 240U).code == render::BackendSubmissionCode::kAccepted);
  assert(composition.registerPreviewSurfaceProvider("unused-preview", std::move(preview)));
  assert(!composition.resizeSurface(320U, 240U));
  assert(composition.state() == runtime::RuntimeCompositionState::kReady);
  assert(composition.surfaceSnapshot().surfaceGeneration == before.surfaceGeneration);
  assert(composition.surfaceSnapshot().width == 256U);
  assert(composition.controlFacade().mountedCanvasTargets().front() == target);
  runtime::RuntimeFrameOpportunity frame{};
  assert(composition.serviceFrameOpportunity(true, frame));
  assert(frame.canonicalFrame.has_value());

  runtime::RuntimeComposition closed;
  assert(closed.open() && closed.bindSurface(256U, 256U));
  auto closing = std::make_unique<ResizeFailureProvider>();
  auto* raw = closing.get();
  assert(closing->resize(256U, 256U).code == render::BackendSubmissionCode::kAccepted);
  assert(closed.registerSurfaceProvider("closing", std::move(closing)));
  assert(closed.resizeSurface(256U, 256U));
  raw->duringResize = [&] { assert(closed.close()); };
  assert(!closed.resizeSurface(320U, 240U));
  assert(closed.state() == runtime::RuntimeCompositionState::kClosed);
  assert(!closed.surfaceSnapshot().available);
  assert(!closed.serviceFrameOpportunity(true, frame));
  for (const bool lose : {false, true}) {
    runtime::RuntimeComposition interrupted;
    assert(interrupted.open() && interrupted.bindSurface(256U, 256U));
    auto provider = std::make_unique<ResizeFailureProvider>();
    auto* providerRaw = provider.get();
    assert(provider->resize(256U, 256U).code == render::BackendSubmissionCode::kAccepted);
    assert(interrupted.registerSurfaceProvider("interrupted", std::move(provider)));
    assert(interrupted.resizeSurface(256U, 256U));
    providerRaw->duringResize = [&] {
      if (lose) assert(interrupted.loseSurface());
      else assert(!interrupted.resizeSurface(400U, 300U));
    };
    assert(!interrupted.resizeSurface(320U, 240U));
    assert(interrupted.state() == runtime::RuntimeCompositionState::kLost);
    assert(!interrupted.surfaceSnapshot().available);
    assert(interrupted.surfaceSnapshot().width == 256U);
    assert(interrupted.rebindSurface({400.0F, 300.0F, 400U, 300U, 1.0F, 1.0F}));
    assert(interrupted.surfaceSnapshot().width == 400U);
  }
}

void brushPixelsAndMetricsFenceUseTheAcceptedLogicalSpace() {
  using namespace canvas;
  for (const float dpr : {1.0F, 1.25F, 2.0F}) {
    runtime::RuntimeComposition composition;
    const auto physical = static_cast<std::uint32_t>(128.0F * dpr);
    assert(composition.open());
    assert(composition.bindSurface({128.0F, 128.0F, physical, physical, dpr, 1.0F}));
    assert(composition.registerSource(92U));
    assert(composition.setPlatformPresentationDeferred(true));
    input::PlatformPointerBatch down;
    down.samples.push_back({92U, 1U, 1U, 1000U, 30.0F, 40.0F, 0.5F, 0.0F, 0.0F,
        {}, {}, input::SampleProvenance::kConfirmedCurrent, input::PointerPhase::kDown});
    auto move = down;
    move.samples.front().sequence = 2U; move.samples.front().timestampNs = 2000U;
    move.samples.front().phase = input::PointerPhase::kMove;
    move.samples.front().x = 80.0F;
    assert(composition.submit(down) && composition.submit(move));
    runtime::RuntimeFrameOpportunity a{};
    assert(composition.serviceFrameOpportunity(false, a) && a.previewWork);
    std::vector<std::uint8_t> pixels(physical * physical * 4U);
    assert(composition.readPreviewRgba(pixels));
    const auto px = static_cast<std::uint32_t>(50.0F * dpr);
    const auto py = static_cast<std::uint32_t>(40.0F * dpr);
    assert(pixels[(py * physical + px) * 4U + 3U] != 0U);
    assert(!composition.finishFrameOpportunity({a.previewWork, false}));
    runtime::RuntimeFrameOpportunity retry{};
    assert(composition.serviceFrameOpportunity(false, retry));
    assert(retry.previewWork == a.previewWork);
    assert(composition.finishFrameOpportunity({retry.previewWork, true}));
    const auto old = composition.surfaceSnapshot();
    assert(composition.resizeSurface({128.0F, 128.0F, physical, physical, dpr, 1.1F}));
    const auto changed = composition.surfaceSnapshot();
    assert(changed.surfaceGeneration == old.surfaceGeneration);
    assert(changed.metricsGeneration == old.metricsGeneration + 1U);
    assert(!composition.finishFrameOpportunity({a.previewWork, true}));
    assert(!composition.receivePlatformPresented({a.canonicalFrame->viewId, a.canonicalFrame->frameId,
        a.canonicalFrame->surfaceGeneration, a.canonicalFrame->metricsGeneration,
        render::PresentOutcome::kPresented, render::PresentEvidenceKind::kPlatformQualified, std::nullopt}));
    runtime::RuntimeFrameOpportunity b{};
    assert(composition.serviceFrameOpportunity(false, b) && b.previewWork);
    assert(b.previewWork != a.previewWork);
    assert(composition.finishFrameOpportunity({b.previewWork, true}));
    const auto target = composition.controlFacade().mountedCanvasTargets().front();
    assert(composition.resizeSurface({128.0F, 128.0F, physical, physical, dpr, 1.1F}));
    assert(composition.surfaceSnapshot().metricsGeneration == changed.metricsGeneration);
    assert(composition.controlFacade().mountedCanvasTargets().front() == target);
  }
}

void logicalInputMatchesCanonicalAndPreviewPixels() {
  canvas::runtime::RuntimeComposition composition;
  assert(composition.open());
  assert(composition.bindSurface({128.0F, 128.0F, 256U, 256U, 2.0F, 1.0F}));
  assert(composition.registerSource(91U));
  assert(composition.setPlatformPresentationDeferred(true));
  auto& facade = composition.controlFacade();
  canvas::runtime::CanvasControlRequest request{};
  request.clientId = 91U; request.requestId = 1U;
  request.target = facade.mountedCanvasTargets().front();
  request.payload.kind = canvas::runtime::CanvasControlPayloadKind::kSelectCreationPreset;
  request.payload.creationPreset = canvas::runtime::CanvasCreationPreset::kRectangle;
  assert(facade.submitCanvasControl(request).state == canvas::runtime::ProductControlState::kQueued);
  canvas::input::PlatformPointerBatch down;
  down.samples.push_back({91U, 1U, 1U, 1000U, 20.0F, 20.0F, 0.5F, 0.0F, 0.0F,
      {}, {}, canvas::input::SampleProvenance::kConfirmedCurrent, canvas::input::PointerPhase::kDown});
  auto move = down;
  move.samples.front().sequence = 2U; move.samples.front().timestampNs = 2000U;
  move.samples.front().phase = canvas::input::PointerPhase::kMove;
  move.samples.front().x = 60.0F; move.samples.front().y = 60.0F;
  assert(composition.submit(down) && composition.submit(move));
  canvas::runtime::RuntimeFrameOpportunity authoring{};
  assert(composition.serviceFrameOpportunity(false, authoring));
  std::vector<std::uint8_t> pixels(256U * 256U * 4U);
  assert(composition.readCanonicalRgba(pixels));
  const auto coloredAt = [&](std::uint32_t x, std::uint32_t y) {
    const auto i = (y * 256U + x) * 4U;
    return pixels[i] < 245U && pixels[i + 3U] > 0U;
  };
  assert(coloredAt(80U, 80U));
  auto up = move; up.samples.front().phase = canvas::input::PointerPhase::kUp;
  up.samples.front().sequence = 3U; up.samples.front().timestampNs = 3000U;
  assert(composition.submit(up));
  canvas::runtime::RuntimeFrameOpportunity opportunity{};
  assert(composition.serviceFrameOpportunity(false, opportunity));
  assert(composition.readCanonicalRgba(pixels));
  assert(coloredAt(80U, 80U));
  const auto identity = *opportunity.canonicalFrame;
  assert(composition.receivePlatformPresented({identity.viewId, identity.frameId,
      identity.surfaceGeneration, identity.metricsGeneration,
      canvas::render::PresentOutcome::kPresented,
      canvas::render::PresentEvidenceKind::kPlatformQualified, std::nullopt}));
  request.requestId = 2U;
  request.payload.kind = canvas::runtime::CanvasControlPayloadKind::kSelectTool;
  request.payload.tool = canvas::runtime::CanvasToolKind::kSelect;
  assert(facade.submitCanvasControl(request).state == canvas::runtime::ProductControlState::kQueued);
  down.samples.front().sequence = 4U; down.samples.front().x = 40.0F; down.samples.front().y = 40.0F;
  up = down; up.samples.front().sequence = 5U; up.samples.front().phase = canvas::input::PointerPhase::kUp;
  assert(composition.submit(down) && composition.submit(up));
  assert(facade.readCanvasControlSnapshot(request.target).selectedObjectCount == 1U);
  canvas::runtime::RuntimeFrameOpportunity selected{};
  assert(composition.serviceFrameOpportunity(false, selected));
  assert(composition.readCanonicalRgba(pixels));
  const auto corner = (40U * 256U + 40U) * 4U;
  // Selection chrome is in the same logical space as the selected object.
  assert(pixels[corner] < 90U && pixels[corner + 1U] > 140U && pixels[corner + 2U] > 150U);
  const auto before = composition.qualificationRuntime().selectedObjectRecord();
  down.samples.front().sequence = 6U;
  move = down; move.samples.front().sequence = 7U; move.samples.front().phase = canvas::input::PointerPhase::kMove;
  move.samples.front().x = 50.0F;
  up = move; up.samples.front().sequence = 8U; up.samples.front().phase = canvas::input::PointerPhase::kUp;
  assert(composition.submit(down) && composition.submit(move) && composition.submit(up));
  const auto after = composition.qualificationRuntime().selectedObjectRecord();
  assert(before && after && after->transform.tx == before->transform.tx + 10.0);
}

void bindingReplacementKeepsPendingBrushHandoff() {
  using namespace canvas;
  for (const bool canonicalFirst : {false, true}) {
    runtime::RuntimeComposition composition;
    assert(composition.open() && composition.bindSurface(128U, 128U));
    assert(composition.registerSource(93U));
    assert(composition.setPlatformPresentationDeferred(true));
    input::PlatformPointerBatch batch;
    batch.samples.push_back({93U, 1U, 1U, 1000U, 20.0F, 30.0F, 0.5F, 0.0F, 0.0F,
        {}, {}, input::SampleProvenance::kConfirmedCurrent, input::PointerPhase::kDown});
    assert(composition.submit(batch));
    batch.samples.front().phase = input::PointerPhase::kMove;
    batch.samples.front().sequence = 2U; batch.samples.front().timestampNs = 2000U;
    batch.samples.front().x = 70.0F;
    assert(composition.submit(batch));
    batch.samples.front().phase = input::PointerPhase::kUp;
    batch.samples.front().sequence = 3U; batch.samples.front().timestampNs = 3000U;
    assert(composition.submit(batch));
    runtime::RuntimeFrameOpportunity old{};
    assert(composition.serviceFrameOpportunity(false, old));
    assert(old.previewWork && old.canonicalFrame && old.pendingCanonicalHandoff == 1U);
    auto candidate = std::make_unique<render::RasterSkiaSurfaceProvider>();
    assert(candidate->resize(128U, 128U).code == render::BackendSubmissionCode::kAccepted);
    assert(composition.registerPreviewSurfaceProvider("pending-handoff-replacement", std::move(candidate)));
    assert(composition.resizeSurface(128U, 128U));
    assert(!composition.finishFrameOpportunity({old.previewWork, true}));
    runtime::RuntimeFrameOpportunity fresh{};
    assert(composition.serviceFrameOpportunity(false, fresh));
    assert(fresh.previewWork && fresh.canonicalFrame && fresh.pendingCanonicalHandoff == 1U);
    const auto presentCanonical = [&] {
      const auto& frame = *fresh.canonicalFrame;
      assert(composition.receivePlatformPresented({frame.viewId, frame.frameId,
          frame.surfaceGeneration, frame.metricsGeneration, render::PresentOutcome::kPresented,
          render::PresentEvidenceKind::kPlatformQualified, std::nullopt}));
    };
    if (canonicalFirst) presentCanonical();
    assert(composition.finishFrameOpportunity({fresh.previewWork, true}));
    if (!canonicalFirst) presentCanonical();
    assert(composition.qualificationRuntime().pendingCanonicalHandoffCount() == 0U);
    assert(!composition.previewActive());
  }
}

void invalidMetricsAndWrongThreadKeepAcceptedResources() {
  using namespace canvas;
  runtime::RuntimeComposition composition;
  assert(composition.open() && composition.bindSurface(128U, 128U));
  const auto before = composition.surfaceSnapshot();
  const auto target = composition.controlFacade().mountedCanvasTargets().front();
  for (const auto bad : {0.0F, -1.0F, std::numeric_limits<float>::infinity(),
                         std::numeric_limits<float>::quiet_NaN()}) {
    assert(!composition.resizeSurface({bad, 128.0F, 128U, 128U, 1.0F, 1.0F}));
    assert(!composition.resizeSurface({128.0F, 128.0F, 128U, 128U, bad, 1.0F}));
    assert(!composition.resizeSurface({128.0F, 128.0F, 128U, 128U, 1.0F, bad}));
  }
  bool accepted = true;
  std::thread wrongOwner([&] { accepted = composition.resizeSurface(256U, 256U); });
  wrongOwner.join();
  assert(!accepted);
  assert(composition.state() == runtime::RuntimeCompositionState::kReady);
  assert(composition.surfaceSnapshot().surfaceGeneration == before.surfaceGeneration);
  assert(composition.surfaceSnapshot().metricsGeneration == before.metricsGeneration);
  assert(composition.controlFacade().mountedCanvasTargets().front() == target);
  auto* provider = composition.qualificationRuntime().activeSurfaceProvider();
  assert(provider->describe().metrics.physicalWidth == 128U);
  for (const auto size : {160U, 192U, 128U}) {
    assert(composition.resizeSurface(size, size));
    assert(composition.surfaceSnapshot().width == size);
    assert(composition.qualificationRuntime().activeSurfaceProvider()->describe().metrics.physicalWidth == size);
    assert(composition.qualificationRuntime().previewSurfaceProvider()->describe().metrics.physicalWidth == size);
  }
}
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

void bindingChangeInvalidatesControlAndResourceResults() {
  using namespace canvas;
  runtime::RuntimeComposition composition;
  assert(composition.open() && composition.bindSurface(128U, 128U));
  assert(composition.bindManifest(makeManifest(1U), 44U));
  const auto id = foundation::ObjectId::fromUint64(1U);
  const auto oldToken = composition.resources().token(id, 0U, 90U);
  assert(oldToken);
  const auto oldTarget = composition.controlFacade().mountedCanvasTargets().front();
  runtime::CanvasControlRequest request{};
  request.clientId = 44U; request.requestId = 1U; request.target = oldTarget;
  request.payload.kind = runtime::CanvasControlPayloadKind::kSelectTool;
  request.payload.tool = runtime::CanvasToolKind::kSelect;
  assert(composition.controlFacade().submitCanvasControl(request).state == runtime::ProductControlState::kQueued);
  assert(composition.resizeSurface({128.0F, 128.0F, 128U, 128U, 1.0F, 1.25F}));
  assert(composition.controlFacade().mountedCanvasTargets().front() != oldTarget);
  assert(!composition.publishResource(*oldToken, {0x01U}));
  assert(composition.lastResourceError() == runtime::ResourcePortError::kStaleBinding);
  assert(composition.ownerPump());
  const auto stale = composition.controlFacade().canvasControlReceipt({44U, 1U});
  assert(stale && stale->state == runtime::CanvasControlReceiptState::kTargetDestroyed);
  assert(!composition.qualificationRuntime().selectionMode());
  assert(composition.notifyResource(id) && composition.ownerPump());
  const auto fresh = composition.resourceReceipt(id);
  assert(fresh && fresh->token.lifetimeGeneration != oldToken->lifetimeGeneration);
  assert(composition.publishResource(fresh->token, {0x01U}) && composition.ownerPump());
  assert(composition.resourceReceipt(id)->state == runtime::ResourceBindingState::kBytesValidated);
  request.requestId = 2U;
  request.target = composition.controlFacade().mountedCanvasTargets().front();
  const auto freshControl = composition.controlFacade().submitCanvasControl(request);
  assert(freshControl.state == runtime::ProductControlState::kQueued);
  assert(composition.ownerPump());
  assert(composition.qualificationRuntime().selectionMode());
  const auto productState = composition.controlFacade().readRuntimeState();
  assert(productState.identity.runtimeGeneration == request.target.runtimeEpoch);
  assert(composition.controlFacade().setSelectionMode(false, 3U,
      productState.identity.runtimeGeneration, 44U).state == runtime::ProductControlState::kQueued);
  assert(composition.ownerPump());
  assert(!composition.qualificationRuntime().selectionMode());
  runtime::ResourceBindingSnapshot exhausted;
  exhausted.lifetimeGeneration = std::numeric_limits<std::uint64_t>::max();
  assert(!exhausted.bind(makeManifest(1U), 1U));
  exhausted.invalidate();
  assert(exhausted.lifetimeGeneration == std::numeric_limits<std::uint64_t>::max());
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
  // Full platform metrics must survive the composition boundary without
  // being collapsed to physical pixels or DPR=1.
  canvas::runtime::RuntimeComposition metricsComposition;
  assert(metricsComposition.open());
  const canvas::render::SurfaceMetrics acceptedMetrics{
      100.0F, 80.0F, 200U, 160U, 2.0F, 1.25F};
  assert(metricsComposition.bindSurface(acceptedMetrics));
  const auto acceptedSnapshot = metricsComposition.surfaceSnapshot();
  assert(acceptedSnapshot.logicalWidth == 100.0F);
  assert(acceptedSnapshot.logicalHeight == 80.0F);
  assert(acceptedSnapshot.width == 200U);
  assert(acceptedSnapshot.height == 160U);
  assert(acceptedSnapshot.devicePixelRatio == 2.0F);
  assert(acceptedSnapshot.displayScale == 1.25F);
  assert(acceptedSnapshot.surfaceGeneration != 0U);
  assert(acceptedSnapshot.metricsGeneration != 0U);
  assert(metricsComposition.resizeSurface(canvas::render::SurfaceMetrics{
      100.0F, 80.0F, 200U, 160U, 2.0F, 1.5F}));
  const auto metricsOnlySnapshot = metricsComposition.surfaceSnapshot();
  assert(metricsOnlySnapshot.metricsGeneration > acceptedSnapshot.metricsGeneration);
  assert(metricsOnlySnapshot.surfaceGeneration == acceptedSnapshot.surfaceGeneration);
  assert(metricsOnlySnapshot.devicePixelRatio == 2.0F);
  assert(metricsOnlySnapshot.displayScale == 1.5F);

  // Registering an input source is not a surface bind. It must not promote a
  // Bound composition to Ready merely because a provider was prepared.
  canvas::runtime::RuntimeComposition preBind;
  assert(preBind.open());
  auto preparedProvider = std::make_unique<canvas::render::RasterSkiaSurfaceProvider>();
  assert(preparedProvider->resize(256U, 256U).code ==
         canvas::render::BackendSubmissionCode::kAccepted);
  assert(preBind.registerSurfaceProvider("prepared", std::move(preparedProvider)));
  assert(!preBind.registerSource(77U));
  assert(preBind.state() == canvas::runtime::RuntimeCompositionState::kBound);
  assert(!preBind.ownerPump());
  assert(preBind.bindSurface({128.0F, 128.0F, 256U, 256U, 2.0F, 1.0F}));
  assert(preBind.surfaceSnapshot().surfaceGeneration == 1U);
  assert(preBind.surfaceSnapshot().metricsGeneration == 1U);
  assert(preBind.registerSource(77U));
  stagedProvidersAndDualRoleFailure();
  bindingReplacementKeepsPendingBrushHandoff();
  invalidMetricsAndWrongThreadKeepAcceptedResources();
  bindingChangeInvalidatesControlAndResourceResults();
  logicalInputMatchesCanonicalAndPreviewPixels();
  failedCandidatesKeepAcceptedBindingAndReentrantCloseAborts();
  brushPixelsAndMetricsFenceUseTheAcceptedLogicalSpace();
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

  // Work identities from separate RuntimeComposition instances must not
  // compare equal or authorize one another's preview completion.
  canvas::runtime::RuntimeComposition instanceA;
  canvas::runtime::RuntimeComposition instanceB;
  assert(instanceA.open() && instanceA.bindSurface(256U, 256U) && instanceA.registerSource(61U));
  assert(instanceB.open() && instanceB.bindSurface(256U, 256U) && instanceB.registerSource(62U));
  assert(instanceA.setPlatformPresentationDeferred(true));
  assert(instanceB.setPlatformPresentationDeferred(true));
  auto aDown = previewDown; aDown.samples.front().source = 61U; aDown.samples.front().pointer = 601U;
  auto aMove = aDown; aMove.samples.front().sequence = 2U; aMove.samples.front().timestampNs = 2'000U; aMove.samples.front().phase = canvas::input::PointerPhase::kMove; aMove.samples.front().x = 80.0F; aMove.samples.front().y = 80.0F;
  auto bDown = aDown; bDown.samples.front().source = 62U; bDown.samples.front().pointer = 602U;
  auto bMove = aMove; bMove.samples.front().source = 62U; bMove.samples.front().pointer = 602U;
  assert(instanceA.submit(aDown) && instanceA.submit(aMove));
  assert(instanceB.submit(bDown) && instanceB.submit(bMove));
  canvas::runtime::RuntimeFrameOpportunity aOpportunity{}, bOpportunity{};
  assert(instanceA.serviceFrameOpportunity(false, aOpportunity));
  assert(instanceB.serviceFrameOpportunity(false, bOpportunity));
  assert(aOpportunity.previewWork.has_value() && bOpportunity.previewWork.has_value());
  assert(aOpportunity.previewWork != bOpportunity.previewWork);
  assert(!instanceB.finishFrameOpportunity({aOpportunity.previewWork, true}));
  assert(instanceB.previewRenderState().dirty);
  assert(instanceB.finishFrameOpportunity({bOpportunity.previewWork, true}));
  assert(!instanceB.finishFrameOpportunity({bOpportunity.previewWork, true}));
  auto replacementPreview = std::make_unique<canvas::render::RasterSkiaSurfaceProvider>();
  assert(replacementPreview->resize(256U, 256U).code == canvas::render::BackendSubmissionCode::kAccepted);
  assert(instanceB.registerPreviewSurfaceProvider("live-replacement", std::move(replacementPreview)));
  assert(instanceB.resizeSurface(256U, 256U));
  assert(instanceB.previewActive());
  canvas::runtime::RuntimeFrameOpportunity replacementWork{};
  assert(instanceB.serviceFrameOpportunity(false, replacementWork));
  assert(replacementWork.previewWork && replacementWork.previewWork != bOpportunity.previewWork);
  assert(!instanceB.finishFrameOpportunity({bOpportunity.previewWork, true}));
  assert(instanceB.finishFrameOpportunity({replacementWork.previewWork, true}));
  assert(instanceB.close());
  assert(!instanceB.finishFrameOpportunity({replacementWork.previewWork, true}));
  assert(instanceA.finishFrameOpportunity({aOpportunity.previewWork, true}));
  assert(!instanceA.finishFrameOpportunity({aOpportunity.previewWork, true}));
  canvas::runtime::RuntimeComposition recreated;
  assert(recreated.open() && recreated.bindSurface(256U, 256U) && recreated.registerSource(62U));
  assert(recreated.setPlatformPresentationDeferred(true));
  assert(recreated.submit(bDown) && recreated.submit(bMove));
  canvas::runtime::RuntimeFrameOpportunity recreatedWork{};
  assert(recreated.serviceFrameOpportunity(false, recreatedWork));
  assert(recreatedWork.previewWork != replacementWork.previewWork);
  assert(!recreated.finishFrameOpportunity({replacementWork.previewWork, true}));
  assert(recreated.previewRenderState().dirty);
  assert(recreated.finishFrameOpportunity({recreatedWork.previewWork, true}));
  return 0;
}
