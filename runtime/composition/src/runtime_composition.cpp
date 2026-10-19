#include "canvas/runtime/runtime_composition.hpp"
#include "canvas/runtime/canvas_runtime_facade.hpp"
#include "canvas/render/image_resource.hpp"
#include "canvas/text/text_layout.hpp"

#include <cstddef>
#include <cstdint>
#include <algorithm>
#include <atomic>
#include <limits>
#include <string>

namespace canvas::runtime {

namespace {
std::atomic<std::uint64_t> g_nextRuntimeInstanceId{1U};
std::uint64_t allocateRuntimeInstanceId() noexcept {
  auto value = g_nextRuntimeInstanceId.load(std::memory_order_relaxed);
  while (value != std::numeric_limits<std::uint64_t>::max()) {
    if (g_nextRuntimeInstanceId.compare_exchange_weak(value, value + 1U,
        std::memory_order_relaxed)) return value;
  }
  return 0U;
}
}  // namespace

RuntimeComposition::RuntimeComposition(render::ViewId viewId)
    : runtime_(std::make_unique<interaction::CanvasEditingRuntime>(viewId)),
      provider_(std::make_unique<MemoryResourceProvider>()),
      textLayoutService_(std::make_unique<canvas::text::RichTextLayoutService>(
          *provider_, canvas::text::LayoutContext{128.0F, 1U})),
      ownerThread_(std::this_thread::get_id()), viewId_(viewId),
      runtimeInstanceId_(allocateRuntimeInstanceId()) {
  runtime_->bindResourceManifest(&resources_.manifest);
#if defined(CANVAS_RENDER_HAS_SKIA)
  imageResolver_ = std::make_unique<canvas::render::ImageResourceResolver>(*provider_);
#endif
  controlFacade_ = std::make_unique<CanvasRuntimeFacade>(
      *runtime_, CanvasTargetKey{1U, viewId.value(), 1U, 1U});
#if defined(CANVAS_RENDER_HAS_SKIA)
  runtime_->bindResourceServices(imageResolver_.get(), textLayoutService_.get());
#else
  runtime_->bindResourceServices(nullptr, textLayoutService_.get());
#endif
}
void RuntimeComposition::refreshControlTarget() noexcept {
  if (controlFacade_ == nullptr) return;
  const auto surface = runtime_->surface();
  controlFacade_->setTarget(CanvasTargetKey{
      resources_.lifetimeGeneration,
      viewId_.value(),
      surface.metricsGeneration == 0U ? 1U : surface.metricsGeneration,
      attachmentEpoch_});
}
bool RuntimeComposition::canAdvanceSurfaceBinding() const noexcept {
  constexpr auto limit = std::numeric_limits<std::uint64_t>::max() - 1U;
  return attachmentEpoch_ < limit && providerEpoch_ < limit &&
      resources_.lifetimeGeneration < limit;
}
void RuntimeComposition::surfaceBindingChanged() noexcept {
  if (attachmentEpoch_ != std::numeric_limits<std::uint64_t>::max()) ++attachmentEpoch_;
  if (providerEpoch_ != std::numeric_limits<std::uint64_t>::max()) ++providerEpoch_;
  resources_.invalidate();
  notifications_.clear();
  resourceReceipts_.clear();
  pendingPreviewCapture_.reset();
  pendingPreviewPresentCount_ = 0U;
  pendingPreviewWorkId_ = 0U;
  pendingPreviewIdentity_.reset();
  pendingCanonicalWork_.reset();
  refreshControlTarget();
}
RuntimeComposition::~RuntimeComposition() { (void)close(); }
RuntimeFacade& RuntimeComposition::controlFacade() noexcept { return *controlFacade_; }
const RuntimeFacade& RuntimeComposition::controlFacade() const noexcept { return *controlFacade_; }
bool RuntimeComposition::transition(RuntimeCompositionState next) noexcept {
  if (state_ == RuntimeCompositionState::kClosed) return false;
  state_ = next; return true;
}
bool RuntimeComposition::canMutate() const noexcept {
  return state_ == RuntimeCompositionState::kReady && !surfaceTransitionActive_;
}
bool RuntimeComposition::open() noexcept {
  return std::this_thread::get_id() == ownerThread_ && runtimeInstanceId_ != 0U &&
      state_ == RuntimeCompositionState::kCreated && transition(RuntimeCompositionState::kBound);
}
bool RuntimeComposition::bindSurface(std::uint32_t width, std::uint32_t height) noexcept {
  return bindSurface(render::SurfaceMetrics{static_cast<float>(width),
                                             static_cast<float>(height), width,
                                             height, 1.0F, 1.0F});
}
bool RuntimeComposition::bindSurface(const render::SurfaceMetrics& metrics) noexcept {
  if (std::this_thread::get_id() != ownerThread_ || surfaceTransitionActive_ ||
      !canAdvanceSurfaceBinding()) return false;
  surfaceTransitionActive_ = true;
  const bool bound = state_ == RuntimeCompositionState::kBound &&
      runtime_->bindSurface(metrics);
  surfaceTransitionActive_ = false;
  if (bound && state_ == RuntimeCompositionState::kBound) {
    state_ = RuntimeCompositionState::kReady;
    ++attachmentEpoch_;
    refreshControlTarget();
    return true;
  }
  if (state_ == RuntimeCompositionState::kClosed) return false;
  if (!bound && state_ == RuntimeCompositionState::kBound && !runtime_->surface().available &&
      runtime_->surface().generation != 0U) {
    state_ = RuntimeCompositionState::kLost;
    surfaceBindingChanged();
  }
  return bound;
}
bool RuntimeComposition::resizeSurface(std::uint32_t width, std::uint32_t height) noexcept {
  return resizeSurface(render::SurfaceMetrics{static_cast<float>(width),
                                               static_cast<float>(height), width,
                                               height, 1.0F, 1.0F});
}
bool RuntimeComposition::resizeSurface(const render::SurfaceMetrics& metrics) noexcept {
  if (std::this_thread::get_id() != ownerThread_) return false;
  if (state_ == RuntimeCompositionState::kClosed ||
      state_ == RuntimeCompositionState::kStopping ||
      state_ == RuntimeCompositionState::kQuiescent) return false;
  if (surfaceTransitionActive_) {
    (void)runtime_->loseSurface();
    if (state_ == RuntimeCompositionState::kReady || state_ == RuntimeCompositionState::kBound) {
      state_ = RuntimeCompositionState::kLost;
      surfaceBindingChanged();
    }
    return false;
  }
  if (!canMutate() || !canAdvanceSurfaceBinding() ||
      metrics.physicalWidth == 0U || metrics.physicalHeight == 0U) return false;
  const auto before = runtime_->surface();
  surfaceTransitionActive_ = true;
  const bool resized = runtime_->resizeSurface(metrics);
  surfaceTransitionActive_ = false;
  if (state_ != RuntimeCompositionState::kReady &&
      state_ != RuntimeCompositionState::kLost) return false;
  if (resized && (before.generation != runtime_->surface().generation ||
                 before.metricsGeneration != runtime_->surface().metricsGeneration)) surfaceBindingChanged();
  else if (!resized && !runtime_->surface().available && state_ != RuntimeCompositionState::kLost) {
    state_ = RuntimeCompositionState::kLost;
    surfaceBindingChanged();
  }
  return resized;
}
bool RuntimeComposition::registerSurfaceProvider(
    std::string profileId,
    std::unique_ptr<render::SkiaSurfaceProvider> provider) noexcept {
  if (std::this_thread::get_id() != ownerThread_ || surfaceTransitionActive_ ||
      (state_ != RuntimeCompositionState::kBound &&
       state_ != RuntimeCompositionState::kLost && !canMutate()) ||
      provider == nullptr || profileId.empty()) return false;
  return runtime_->stageSurfaceProvider(std::move(profileId), std::move(provider), false);
}
bool RuntimeComposition::registerPreviewSurfaceProvider(
    std::string profileId,
    std::unique_ptr<render::SkiaSurfaceProvider> provider) noexcept {
  if (std::this_thread::get_id() != ownerThread_ || surfaceTransitionActive_ ||
      (state_ != RuntimeCompositionState::kBound &&
       state_ != RuntimeCompositionState::kLost && !canMutate()) ||
      provider == nullptr || profileId.empty()) return false;
  return runtime_->stageSurfaceProvider(std::move(profileId), std::move(provider), true);
}
bool RuntimeComposition::refreshManifestDerivedState() noexcept {
  runtime_->bindResourceManifest(&resources_.manifest);
  runtime_->refreshResourceManifestServices();
  return true;
}
bool RuntimeComposition::bindManifest(const semantic::ResourceManifest& manifest, std::uint64_t binding) noexcept {
  if (std::this_thread::get_id() != ownerThread_) { lastResourceError_ = ResourcePortError::kWrongThread; return false; }
  const auto previous = resources_.manifest;
  if ((state_ != RuntimeCompositionState::kBound && !canMutate()) || !resources_.bind(manifest, binding)) {
    lastResourceError_ = ResourcePortError::kValidationFailed; return false;
  }
  for (const auto& oldEntry : previous.entries) {
    if (manifest.entries.end() == std::find_if(manifest.entries.begin(), manifest.entries.end(),
        [&](const auto& entry) { return entry.resource_id == oldEntry.resource_id; })) {
      provider_->markMissing(oldEntry.resource_id.value);
    }
  }
  notifications_.clear();
  resourceReceipts_.clear();
  for (auto it = resourceReceipts_.begin(); it != resourceReceipts_.end();) {
    if (resources_.find(it->first) == nullptr) it = resourceReceipts_.erase(it);
    else ++it;
  }
  (void)refreshManifestDerivedState();
  lastResourceError_ = ResourcePortError::kNone;
  return true;
}
bool RuntimeComposition::registerSource(input::InputSourceId source) noexcept {
  if (std::this_thread::get_id() != ownerThread_) return false;
  return canMutate() &&
      runtime_->registerInputSource(source, "composition");
}
bool RuntimeComposition::unregisterSource(input::InputSourceId source,
                                           std::uint64_t sequence,
                                           std::uint64_t timestampNs) noexcept {
  if (std::this_thread::get_id() != ownerThread_ || surfaceTransitionActive_ || !canMutate()) return false;
  return runtime_->unregisterInputSource(source, sequence, timestampNs);
}
bool RuntimeComposition::cancelAllPointers() noexcept {
  if (std::this_thread::get_id() != ownerThread_ || !canMutate()) return false;
  runtime_->cancelAllPointers();
  return true;
}
bool RuntimeComposition::submit(const input::PlatformPointerBatch& batch) noexcept {
  if (std::this_thread::get_id() != ownerThread_ || !canMutate() || batch.samples.empty()) return false;
  // Controls submitted through the public RuntimeFacade share this owner
  // boundary. Consume them before the next platform input batch so a host
  // cannot observe one gesture under stale tool/creation state.
  if (controlFacade_ != nullptr && std::this_thread::get_id() == ownerThread_) {
    ++ownerPumpCount_;
    (void)controlFacade_->process(ownerPumpCount_);
  }
  return
      runtime_->acceptPlatformBatch(batch, batch.samples.back().timestampNs);
}
semantic::ApplyResult RuntimeComposition::applyExternalOperation(
    const semantic::Operation& operation, semantic::ApplySource source) noexcept {
  if (!canMutate() || std::this_thread::get_id() != ownerThread_) return {};
  return runtime_->applyExternalOperation(operation, source);
}
bool RuntimeComposition::notifyResource(foundation::ObjectId id) noexcept {
  if (std::this_thread::get_id() != ownerThread_) { lastResourceError_ = ResourcePortError::kWrongThread; return false; }
  if (!canMutate()) { lastResourceError_ = ResourcePortError::kNotReady; return false; }
  const auto token = resources_.token(id, providerEpoch_, nextRequestId_++);
  if (!token) { lastResourceError_ = ResourcePortError::kUnknownResource; return false; }
  for (auto& notification : notifications_) {
    if (notification.resourceId != id) continue;
    const auto currentToken = resources_.token(id, notification.token.providerEpoch,
                                               notification.token.requestId);
    if (currentToken && notification.token == *currentToken &&
        notification.token.documentBinding == token->documentBinding &&
        notification.token.resourceVersion == token->resourceVersion &&
        notification.token.expectedSha256 == token->expectedSha256) {
      lastResourceError_ = ResourcePortError::kNone;
      return true;
    }
    notification.token = *token;
    lastResourceError_ = ResourcePortError::kNone;
    return true;
  }
  if (notifications_.size() >= 256U) { lastResourceError_ = ResourcePortError::kQueueFull; return false; }
  notifications_.push_back({id, *token});
  lastResourceError_ = ResourcePortError::kNone;
  return true;
}
bool RuntimeComposition::publishResource(foundation::ObjectId id, std::vector<std::uint8_t> bytes) noexcept {
  if (std::this_thread::get_id() != ownerThread_) { lastResourceError_ = ResourcePortError::kWrongThread; return false; }
  if (!canMutate()) { lastResourceError_ = ResourcePortError::kNotReady; return false; }
  // An id-only completion is safe only for the first publication. Once the
  // provider has bytes, callers must retain and present the binding token so
  // a late completion from an older manifest cannot overwrite the current
  // resource under a new binding.
  const auto existing = provider_->resolve(id);
  if (existing.generation != 0U) {
    lastResourceError_ = ResourcePortError::kStaleBinding;
    return false;
  }
  const auto token = resources_.token(id, providerEpoch_, nextRequestId_);
  if (!token) { lastResourceError_ = ResourcePortError::kUnknownResource; return false; }
  return publishResource(*token, std::move(bytes));
}
bool RuntimeComposition::publishResource(const ResourceBindingToken& token,
                                         std::vector<std::uint8_t> bytes) noexcept {
  if (std::this_thread::get_id() != ownerThread_) { lastResourceError_ = ResourcePortError::kWrongThread; return false; }
  if (!canMutate()) { lastResourceError_ = ResourcePortError::kNotReady; return false; }
  const auto* entry = resources_.find(token.resourceId);
  const bool tokenMatches = token.documentBinding == resources_.documentBinding &&
      token.lifetimeGeneration == resources_.lifetimeGeneration && entry != nullptr &&
      token.providerEpoch == providerEpoch_ &&
      token.resourceVersion == entry->resource_revision &&
      token.expectedSha256 == entry->content_hash;
  if (!tokenMatches) { lastResourceError_ = ResourcePortError::kStaleBinding; return false; }
  const EncodedResource candidate{
      0U, std::make_shared<const std::vector<std::uint8_t>>(bytes)};
  if (!validateResourceBytes(*entry, candidate)) {
    lastResourceError_ = ResourcePortError::kValidationFailed;
    return false;
  }
  if (notifications_.size() >= 256U) { lastResourceError_ = ResourcePortError::kQueueFull; return false; }
  provider_->publish(token.resourceId, std::move(bytes));
  auto completion = token;
  completion.providerEpoch = providerEpoch_;
  completion.requestId = nextRequestId_++;
  notifications_.push_back({token.resourceId, completion});
  lastResourceError_ = ResourcePortError::kNone;
  return true;
}
bool RuntimeComposition::ownerPump() noexcept {
  if (std::this_thread::get_id() != ownerThread_) { lastResourceError_ = ResourcePortError::kWrongThread; return false; }
  if (!canMutate()) { lastResourceError_ = ResourcePortError::kNotReady; return false; }
  ++ownerPumpCount_;
  lastOwnerControlCount_ = 0;
  if (controlFacade_ != nullptr) {
    lastOwnerControlCount_ = controlFacade_->process(ownerPumpCount_);
  }
  bool derivedRefresh = false;
  for (const auto& notification : notifications_) {
    const auto* entry = resources_.find(notification.resourceId);
    const auto pulled = provider_->resolve(notification.resourceId);
    ResourcePortReceipt receipt{};
    receipt.resourceId = notification.resourceId;
    receipt.providerGeneration = pulled.generation;
    receipt.byteLength = pulled.bytes ? pulled.bytes->size() : 0U;
    receipt.token = notification.token;
    receipt.state = entry == nullptr ? ResourceBindingState::kMissing
        : !pulled.bytes ? ResourceBindingState::kMissing
        : !(notification.token == resources_.token(notification.resourceId, notification.token.providerEpoch, notification.token.requestId).value_or(ResourceBindingToken{})) ? ResourceBindingState::kProviderReplaced
        : validateResourceBytes(*entry, pulled) ? ResourceBindingState::kBytesValidated
                                                : ResourceBindingState::kFailed;
    const auto old = resourceReceipts_.find(notification.resourceId);
    receipt.changed = old == resourceReceipts_.end() || old->second.providerGeneration != receipt.providerGeneration || old->second.state != receipt.state || old->second.token != receipt.token;
    const bool changed = receipt.changed;
    resourceReceipts_[notification.resourceId] = std::move(receipt);
    derivedRefresh = derivedRefresh || changed;
  }
  notifications_.clear();
  if (derivedRefresh) {
    // Resource publication is derived state. It must request a canonical
    // redraw at the owner safe point without creating a document operation.
    if (!runtime_->refreshResourceDerivedState()) return false;
  }
  return true;
}
bool RuntimeComposition::requestCanonicalFrame(std::uint64_t frameId) noexcept {
  if (!canMutate() || std::this_thread::get_id() != ownerThread_ ||
      nextCanonicalWorkId_ == std::numeric_limits<std::uint64_t>::max()) return false;
  // A frame opportunity is also the shared owner safe point. This advances
  // accepted controls and resource notifications even when the surface is
  // idle and no pointer batch arrives (for example Web rAF/present).
  // Suppress implicit automatic presents while the owner pump applies those
  // controls. The public frame opportunity owns the frame identity and must
  // submit it exactly once, regardless of the Runtime's normal presentation
  // mode. Restore the mode before the public submission and on failure.
  const bool wasDeferred = runtime_->platformPresentationDeferred();
  runtime_->setPlatformPresentationDeferred(true);
  const bool pumped = ownerPump();
  runtime_->setPlatformPresentationDeferred(wasDeferred);
  if (!pumped) return false;
  return runtime_->presentCanonicalFrame(frameId, 0.0, false) && captureCanonicalWork();
}
bool RuntimeComposition::serviceFrameOpportunity(
    bool forceCanonical, RuntimeFrameOpportunity& out) noexcept {
  out = {};
  if (std::this_thread::get_id() != ownerThread_ || !canMutate()) return false;
  // A capture may be superseded by input, resize, restore, or cancellation
  // before the platform reports its presentation result.  Invalidate that
  // obsolete work at the next owner opportunity so it cannot reserve the
  // single in-flight slot indefinitely.
  if (pendingPreviewCapture_.has_value()) {
    const auto& state = runtime_->previewRenderState();
    if (!runtime_->previewActive() ||
        !pendingPreviewIdentity_ ||
        pendingPreviewIdentity_->lifetimeGeneration != resources_.lifetimeGeneration ||
        pendingPreviewIdentity_->metricsGeneration != runtime_->surface().metricsGeneration ||
        state.contentRevision != pendingPreviewCapture_->contentRevision ||
        runtime_->previewSurfaceGeneration() !=
            pendingPreviewCapture_->surfaceGeneration) {
      pendingPreviewCapture_.reset();
      pendingPreviewPresentCount_ = 0U;
      pendingPreviewWorkId_ = 0U;
      pendingPreviewIdentity_.reset();
    }
  }
  // Consume controls and resource notifications first. Their application may
  // create a new canonical demand during this same frame opportunity.
  if (!ownerPump()) return false;
  const bool shouldCanonical = forceCanonical || runtime_->canonicalPresentationDirty();
  if (nextCanonicalWorkId_ == std::numeric_limits<std::uint64_t>::max()) return false;
  if (shouldCanonical) {
    const auto frameId = runtime_->nextCanonicalFrameId();
    if (frameId == 0U) return false;
    out.canonicalRequested = runtime_->presentCanonicalFrame(frameId, 0.0, false);
    if (!out.canonicalRequested && !runtime_->canonicalPresentationRetryable()) return false;
  }
  if (!captureCanonicalWork()) return false;
  out.previewActive = runtime_->previewActive();
  out.previewDirty = runtime_->previewRenderState().dirty;
  if (out.previewActive && out.previewDirty && !pendingPreviewCapture_.has_value()) {
    if (nextPreviewWorkId_ == std::numeric_limits<std::uint64_t>::max()) return false;
    interaction::PreviewPresentationCapture capture{};
    if (runtime_->capturePreviewPresentation(capture)) {
      std::uint64_t presentCount = 0U;
      if (runtime_->renderPreviewPresentation(capture, &presentCount)) {
        pendingPreviewCapture_ = std::move(capture);
        pendingPreviewPresentCount_ = presentCount;
        pendingPreviewWorkId_ = nextPreviewWorkId_++;
        pendingPreviewIdentity_ = RuntimeFrameOpportunity::PreviewWork{
            runtimeInstanceId_, viewId_.value(), resources_.lifetimeGeneration,
            pendingPreviewWorkId_, pendingPreviewCapture_->contentRevision,
            pendingPreviewCapture_->surfaceGeneration, runtime_->surface().metricsGeneration};
      } else if (!runtime_->previewPresentationRetryable()) {
        return false;
      }
    }
  }
  out.previewRendered = pendingPreviewCapture_.has_value();
  if (pendingPreviewCapture_.has_value()) {
    out.previewWork = pendingPreviewIdentity_;
  }
  out.pendingCanonicalHandoff = runtime_->pendingCanonicalHandoffCount();
  out.previewSurfaceGeneration = runtime_->previewSurfaceGeneration();
  out.previewRevision = runtime_->previewRenderState().contentRevision;
  out.canonicalFrame = runtime_->lastSubmittedCanonicalFrame();
  out.canonicalWork = lastSubmittedCanonicalWork();
  out.needsPaint = out.canonicalRequested || out.previewDirty ||
      out.pendingCanonicalHandoff != 0U || out.canonicalFrame.has_value();
  return true;
}
bool RuntimeComposition::finishFrameOpportunity(
    const RuntimeFrameCompletion& completion) noexcept {
  if (std::this_thread::get_id() != ownerThread_ || !canMutate()) return false;
  if (!pendingPreviewCapture_.has_value()) return !completion.previewWork.has_value();
  if (!completion.previewWork.has_value() ||
      completion.previewWork != pendingPreviewIdentity_ ||
      completion.previewWork->runtimeInstanceId != runtimeInstanceId_ ||
      completion.previewWork->viewId != viewId_.value() ||
      completion.previewWork->lifetimeGeneration != resources_.lifetimeGeneration ||
      completion.previewWork->workId != pendingPreviewWorkId_ ||
      completion.previewWork->contentRevision != pendingPreviewCapture_->contentRevision ||
      completion.previewWork->surfaceGeneration != pendingPreviewCapture_->surfaceGeneration ||
      completion.previewWork->metricsGeneration != runtime_->surface().metricsGeneration) {
    return false;
  }
  const auto& state = runtime_->previewRenderState();
  const bool superseded = !runtime_->previewActive() ||
      state.contentRevision != pendingPreviewCapture_->contentRevision ||
      runtime_->previewSurfaceGeneration() != pendingPreviewCapture_->surfaceGeneration;
  if (superseded) {
    pendingPreviewCapture_.reset();
    pendingPreviewPresentCount_ = 0U;
    pendingPreviewWorkId_ = 0U;
    pendingPreviewIdentity_.reset();
    return false;
  }
  if (!completion.platformPresented) return false;
  const bool accepted = runtime_->acknowledgePreviewPresentation(
      *pendingPreviewCapture_, pendingPreviewPresentCount_);
  if (accepted) {
    pendingPreviewCapture_.reset();
    pendingPreviewPresentCount_ = 0U;
    pendingPreviewWorkId_ = 0U;
    pendingPreviewIdentity_.reset();
  } else {
    // A newer move, resize, restore, or cancellation may have advanced the
    // preview identity while the platform was presenting this capture.  The
    // strict acknowledgement check must remain intact, but an obsolete
    // capture cannot pin the next dirty opportunity forever.  Drop only when
    // the identity has actually been superseded; a plain platform retry keeps
    // the in-flight work available for another presentation attempt.
  }
  return accepted;
}
bool RuntimeComposition::captureCanonicalWork() noexcept {
  const auto frame = runtime_->lastSubmittedCanonicalFrame();
  if (!frame) { pendingCanonicalWork_.reset(); return true; }
  if (lastSubmittedCanonicalWork()) return true;
  if (runtimeInstanceId_ == 0U ||
      nextCanonicalWorkId_ == std::numeric_limits<std::uint64_t>::max()) return false;
  pendingCanonicalWork_ = CanonicalWork{runtimeInstanceId_, resources_.lifetimeGeneration,
      attachmentEpoch_, nextCanonicalWorkId_++, *frame};
  return true;
}
std::optional<CanonicalWork> RuntimeComposition::lastSubmittedCanonicalWork() const noexcept {
  if (!canMutate() || !pendingCanonicalWork_ ||
      pendingCanonicalWork_->runtimeInstanceId != runtimeInstanceId_ ||
      pendingCanonicalWork_->lifetimeGeneration != resources_.lifetimeGeneration ||
      pendingCanonicalWork_->attachmentEpoch != attachmentEpoch_) return std::nullopt;
  const auto current = runtime_->lastSubmittedCanonicalFrame();
  if (!current || *current != pendingCanonicalWork_->frame) return std::nullopt;
  return pendingCanonicalWork_;
}
bool RuntimeComposition::receivePlatformPresented(const CanonicalCompletion& completion) noexcept {
  if (std::this_thread::get_id() != ownerThread_ || !canMutate()) return false;
  const auto current = lastSubmittedCanonicalWork();
  if (!current || completion.work != *current) return false;
  const auto& frame = current->frame;
  const bool accepted = runtime_->receivePlatformPresented({frame.viewId, frame.frameId,
      frame.surfaceGeneration, frame.metricsGeneration, completion.outcome,
      completion.evidence, completion.presentedTimeNs});
  if (accepted) pendingCanonicalWork_.reset();
  return accepted;
}
bool RuntimeComposition::loseSurface() noexcept {
  if (std::this_thread::get_id() != ownerThread_ ||
      state_ != RuntimeCompositionState::kReady || !runtime_->loseSurface()) return false;
  state_ = RuntimeCompositionState::kLost;
  surfaceBindingChanged();
  return true;
}
bool RuntimeComposition::rebindSurface() noexcept {
  const auto surface = surfaceSnapshot();
  return rebindSurface(render::SurfaceMetrics{surface.logicalWidth, surface.logicalHeight,
      surface.width, surface.height, surface.devicePixelRatio, surface.displayScale});
}
bool RuntimeComposition::rebindSurface(const render::SurfaceMetrics& metrics) noexcept {
  if (std::this_thread::get_id() != ownerThread_ || surfaceTransitionActive_ ||
      state_ != RuntimeCompositionState::kLost || !canAdvanceSurfaceBinding()) return false;
  surfaceTransitionActive_ = true;
  const bool rebound = runtime_->rebindSurface(metrics);
  surfaceTransitionActive_ = false;
  if (!rebound || state_ != RuntimeCompositionState::kLost) return false;
  state_ = RuntimeCompositionState::kReady;
  surfaceBindingChanged();
  return true;
}
bool RuntimeComposition::adoptPreparedSurface(
    std::uint64_t expectedGeneration, const render::SurfaceMetrics& metrics) noexcept {
  if (expectedGeneration == 0U || expectedGeneration != runtime_->surface().generation) return false;
  // Expected identity is the last accepted binding, not a host-invented future
  // generation. Both prepared candidates are committed by the same path.
  return rebindSurface(metrics);
}
bool RuntimeComposition::stop() noexcept {
  if (std::this_thread::get_id() != ownerThread_ ||
      (state_ != RuntimeCompositionState::kReady && state_ != RuntimeCompositionState::kLost)) return false;
  resources_.invalidate();
  notifications_.clear();
  pendingPreviewCapture_.reset();
  pendingPreviewIdentity_.reset();
  pendingCanonicalWork_.reset();
  runtime_->close();
  return transition(RuntimeCompositionState::kStopping) && transition(RuntimeCompositionState::kQuiescent);
}
bool RuntimeComposition::detach() noexcept {
  if (std::this_thread::get_id() != ownerThread_ || state_ != RuntimeCompositionState::kQuiescent) return false;
  resources_.invalidate();
  notifications_.clear();
  return true;
}
bool RuntimeComposition::close() noexcept { if (std::this_thread::get_id() != ownerThread_) return false; if (state_ == RuntimeCompositionState::kClosed) return true; if (state_ == RuntimeCompositionState::kReady || state_ == RuntimeCompositionState::kLost) (void)stop(); else runtime_->close(); resources_.invalidate(); notifications_.clear(); state_ = RuntimeCompositionState::kClosed; return true; }
std::uint64_t RuntimeComposition::canonicalPresentCount() const noexcept {
  const auto* provider = runtime_->activeSurfaceProvider();
  return provider == nullptr ? 0U : provider->presentCount();
}
RuntimeCompositionReceipt RuntimeComposition::readback() const noexcept {
  const auto& runtime = *runtime_;
  return {state_, resources_.lifetimeGeneration, ownerPumpCount_, lastOwnerControlCount_, resources_.manifest.manifest_revision, runtime.sceneRevision(), runtime.semanticGeneration().value(), notifications_.size(), runtime.lastSubmittedCanonicalFrame().has_value(), runtime.canonicalPresentationDirty()};
}

RuntimeSurfaceSnapshot RuntimeComposition::surfaceSnapshot() const noexcept {
  const auto surface = runtime_->surface();
  RuntimeSurfaceSnapshot out{};
  out.viewId = viewId_;
  out.surfaceGeneration = surface.generation;
  out.metricsGeneration = surface.metricsGeneration;
  out.logicalWidth = surface.logicalWidth;
  out.logicalHeight = surface.logicalHeight;
  out.width = surface.width;
  out.height = surface.height;
  out.devicePixelRatio = surface.devicePixelRatio;
  out.displayScale = surface.displayScale;
  out.available = surface.available;
  out.backend = canonicalBackend();
  return out;
}

std::uint64_t RuntimeComposition::previewSurfaceGeneration() const noexcept {
  return runtime_->previewSurfaceGeneration();
}

render::RenderTargetBackend RuntimeComposition::canonicalBackend() const noexcept {
  const auto* provider = const_cast<interaction::CanvasEditingRuntime&>(*runtime_)
      .activeSurfaceProvider();
  return provider == nullptr ? render::RenderTargetBackend::kRaster
                             : provider->describe().backend;
}

std::uint64_t RuntimeComposition::nextCanonicalFrameId() const noexcept {
  return runtime_->nextCanonicalFrameId();
}
std::uint64_t RuntimeComposition::canonicalContentSerial() const noexcept {
  return runtime_->canonicalFrameCount();
}
std::optional<render::RuntimeFrameIdentity>
RuntimeComposition::lastSubmittedCanonicalFrame() const noexcept {
  return runtime_->lastSubmittedCanonicalFrame();
}
bool RuntimeComposition::canonicalPresentationDirty() const noexcept {
  return runtime_->canonicalPresentationDirty();
}
bool RuntimeComposition::previewActive() const noexcept { return runtime_->previewActive(); }
const render::SurfaceRenderState& RuntimeComposition::previewRenderState() const noexcept {
  return runtime_->previewRenderState();
}
bool RuntimeComposition::capturePreviewPresentation(
    interaction::PreviewPresentationCapture& capture) const noexcept {
  return const_cast<interaction::CanvasEditingRuntime&>(*runtime_)
      .capturePreviewPresentation(capture);
}
bool RuntimeComposition::renderPreviewPresentation(
    const interaction::PreviewPresentationCapture& capture,
    std::uint64_t* presentCount) noexcept {
  if (std::this_thread::get_id() != ownerThread_) return false;
  return runtime_->renderPreviewPresentation(capture, presentCount);
}
bool RuntimeComposition::acknowledgePreviewPresentation(
    const interaction::PreviewPresentationCapture& capture,
    std::uint64_t presentCount) noexcept {
  if (std::this_thread::get_id() != ownerThread_) return false;
  return runtime_->acknowledgePreviewPresentation(capture, presentCount);
}
bool RuntimeComposition::readCanonicalRgba(std::span<std::uint8_t> destination) noexcept {
  if (std::this_thread::get_id() != ownerThread_ || !canMutate()) return false;
  auto* provider = runtime_->activeSurfaceProvider();
  return provider != nullptr &&
      provider->readbackRgba(destination).code == render::BackendSubmissionCode::kAccepted;
}
bool RuntimeComposition::readPreviewRgba(std::span<std::uint8_t> destination) noexcept {
  if (std::this_thread::get_id() != ownerThread_ || !canMutate()) return false;
  auto* provider = runtime_->previewSurfaceProvider();
  return provider != nullptr &&
      provider->readbackRgba(destination).code == render::BackendSubmissionCode::kAccepted;
}
bool RuntimeComposition::canonicalProviderAvailable() const noexcept {
  return canMutate() && runtime_->surface().available && runtime_->activeSurfaceProvider() != nullptr;
}
bool RuntimeComposition::previewProviderAvailable() const noexcept {
  return canMutate() && runtime_->surface().available && runtime_->previewSurfaceProvider() != nullptr;
}
bool RuntimeComposition::setPlatformPresentationDeferred(bool deferred) noexcept {
  if (std::this_thread::get_id() != ownerThread_ || !canMutate()) return false;
  runtime_->setPlatformPresentationDeferred(deferred);
  return true;
}
std::optional<ResourcePortReceipt> RuntimeComposition::resourceReceipt(
    foundation::ObjectId id) const noexcept {
  if (resources_.find(id) == nullptr) return std::nullopt;
  const auto it = resourceReceipts_.find(id);
  return it == resourceReceipts_.end() ? std::nullopt : std::optional<ResourcePortReceipt>(it->second);
}
}  // namespace canvas::runtime
