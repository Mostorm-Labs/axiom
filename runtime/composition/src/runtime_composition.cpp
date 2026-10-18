#include "canvas/runtime/runtime_composition.hpp"
#include "canvas/runtime/canvas_runtime_facade.hpp"
#include "canvas/render/image_resource.hpp"
#include "canvas/text/text_layout.hpp"

#include <cstddef>
#include <cstdint>
#include <algorithm>
#include <string>

namespace canvas::runtime {

RuntimeComposition::RuntimeComposition(render::ViewId viewId)
    : runtime_(std::make_unique<interaction::CanvasEditingRuntime>(viewId)),
      provider_(std::make_unique<MemoryResourceProvider>()),
      textLayoutService_(std::make_unique<canvas::text::RichTextLayoutService>(
          *provider_, canvas::text::LayoutContext{128.0F, 1U})),
      ownerThread_(std::this_thread::get_id()), viewId_(viewId) {
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
      surface.generation == 0U ? 1U : surface.generation,
      surface.generation == 0U ? 1U : surface.generation});
}
RuntimeComposition::~RuntimeComposition() { (void)close(); }
RuntimeFacade& RuntimeComposition::controlFacade() noexcept { return *controlFacade_; }
const RuntimeFacade& RuntimeComposition::controlFacade() const noexcept { return *controlFacade_; }
bool RuntimeComposition::transition(RuntimeCompositionState next) noexcept {
  if (state_ == RuntimeCompositionState::kClosed) return false;
  state_ = next; return true;
}
bool RuntimeComposition::canMutate() const noexcept { return state_ == RuntimeCompositionState::kReady; }
bool RuntimeComposition::open() noexcept {
  return state_ == RuntimeCompositionState::kCreated && transition(RuntimeCompositionState::kBound);
}
bool RuntimeComposition::bindSurface(std::uint32_t width, std::uint32_t height) noexcept {
  if (std::this_thread::get_id() != ownerThread_) return false;
  const bool bound = state_ == RuntimeCompositionState::kBound &&
      runtime_->bindSurface(width, height) && transition(RuntimeCompositionState::kReady);
  if (bound) refreshControlTarget();
  return bound;
}
bool RuntimeComposition::resizeSurface(std::uint32_t width, std::uint32_t height) noexcept {
  if (std::this_thread::get_id() != ownerThread_ || !canMutate() || width == 0U || height == 0U) return false;
  const bool resized = runtime_->resizeSurface(width, height);
  if (resized) refreshControlTarget();
  return resized;
}
bool RuntimeComposition::registerSurfaceProvider(
    std::string profileId,
    std::unique_ptr<render::SkiaSurfaceProvider> provider) noexcept {
  if (std::this_thread::get_id() != ownerThread_ ||
      (state_ != RuntimeCompositionState::kBound &&
       state_ != RuntimeCompositionState::kLost && !canMutate()) ||
      provider == nullptr || profileId.empty()) return false;
  return runtime_->registerSurfaceProvider(std::move(profileId), std::move(provider));
}
bool RuntimeComposition::registerPreviewSurfaceProvider(
    std::string profileId,
    std::unique_ptr<render::SkiaSurfaceProvider> provider) noexcept {
  if (std::this_thread::get_id() != ownerThread_ ||
      (state_ != RuntimeCompositionState::kBound &&
       state_ != RuntimeCompositionState::kLost && !canMutate()) ||
      provider == nullptr || profileId.empty()) return false;
  return runtime_->registerPreviewSurfaceProvider(std::move(profileId), std::move(provider));
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
  if (state_ == RuntimeCompositionState::kBound && runtime_->surface().available) {
    state_ = RuntimeCompositionState::kReady;
  }
  return (state_ == RuntimeCompositionState::kBound || canMutate()) &&
      runtime_->registerInputSource(source, "composition");
}
bool RuntimeComposition::unregisterSource(input::InputSourceId source,
                                           std::uint64_t sequence,
                                           std::uint64_t timestampNs) noexcept {
  if (std::this_thread::get_id() != ownerThread_ || !canMutate()) return false;
  return runtime_->unregisterInputSource(source, sequence, timestampNs);
}
bool RuntimeComposition::cancelAllPointers() noexcept {
  if (std::this_thread::get_id() != ownerThread_ || !canMutate()) return false;
  runtime_->cancelAllPointers();
  return true;
}
bool RuntimeComposition::submit(const input::PlatformPointerBatch& batch) noexcept {
  if (state_ == RuntimeCompositionState::kBound && runtime_->surface().available) {
    state_ = RuntimeCompositionState::kReady;
  }
  if (!canMutate() || batch.samples.empty()) return false;
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
  if (state_ == RuntimeCompositionState::kBound && runtime_->surface().available) {
    state_ = RuntimeCompositionState::kReady;
  }
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
  if (!canMutate() || std::this_thread::get_id() != ownerThread_) return false;
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
  return runtime_->presentCanonicalFrame(frameId, 0.0, false);
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
        state.contentRevision != pendingPreviewCapture_->contentRevision ||
        runtime_->previewSurfaceGeneration() !=
            pendingPreviewCapture_->surfaceGeneration) {
      pendingPreviewCapture_.reset();
      pendingPreviewPresentCount_ = 0U;
    }
  }
  // Consume controls and resource notifications first. Their application may
  // create a new canonical demand during this same frame opportunity.
  if (!ownerPump()) return false;
  const bool shouldCanonical = forceCanonical || runtime_->canonicalPresentationDirty();
  if (shouldCanonical) {
    const auto frameId = runtime_->nextCanonicalFrameId();
    if (frameId == 0U || !runtime_->presentCanonicalFrame(frameId, 0.0, false)) return false;
    out.canonicalRequested = true;
  }
  out.previewActive = runtime_->previewActive();
  out.previewDirty = runtime_->previewRenderState().dirty;
  if (out.previewActive && out.previewDirty && !pendingPreviewCapture_.has_value()) {
    interaction::PreviewPresentationCapture capture{};
    if (runtime_->capturePreviewPresentation(capture)) {
      std::uint64_t presentCount = 0U;
      if (!runtime_->renderPreviewPresentation(capture, &presentCount)) return false;
      pendingPreviewCapture_ = capture;
      pendingPreviewPresentCount_ = presentCount;
    }
  }
  out.previewRendered = pendingPreviewCapture_.has_value();
  out.pendingCanonicalHandoff = runtime_->pendingCanonicalHandoffCount();
  out.previewSurfaceGeneration = runtime_->previewSurfaceGeneration();
  out.previewRevision = runtime_->previewRenderState().contentRevision;
  out.canonicalFrame = runtime_->lastSubmittedCanonicalFrame();
  out.needsPaint = out.canonicalRequested || out.previewDirty ||
      out.pendingCanonicalHandoff != 0U || out.canonicalFrame.has_value();
  return true;
}
bool RuntimeComposition::finishFrameOpportunity(bool platformPresented) noexcept {
  if (std::this_thread::get_id() != ownerThread_) return false;
  if (!pendingPreviewCapture_.has_value()) return true;
  const auto& state = runtime_->previewRenderState();
  const bool superseded = !runtime_->previewActive() ||
      state.contentRevision != pendingPreviewCapture_->contentRevision ||
      runtime_->previewSurfaceGeneration() != pendingPreviewCapture_->surfaceGeneration;
  if (superseded) {
    pendingPreviewCapture_.reset();
    pendingPreviewPresentCount_ = 0U;
    return false;
  }
  if (!platformPresented) return false;
  const bool accepted = runtime_->acknowledgePreviewPresentation(
      *pendingPreviewCapture_, pendingPreviewPresentCount_);
  if (accepted) {
    pendingPreviewCapture_.reset();
    pendingPreviewPresentCount_ = 0U;
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
bool RuntimeComposition::receivePlatformPresented(const render::PresentedFeedback& feedback) noexcept {
  return std::this_thread::get_id() == ownerThread_ && canMutate() &&
      runtime_->receivePlatformPresented(feedback);
}
bool RuntimeComposition::loseSurface() noexcept {
  if (std::this_thread::get_id() != ownerThread_ ||
      state_ != RuntimeCompositionState::kReady || !runtime_->loseSurface()) return false;
  resources_.invalidate();
  notifications_.clear();
  return transition(RuntimeCompositionState::kLost);
}
bool RuntimeComposition::rebindSurface() noexcept {
  if (std::this_thread::get_id() != ownerThread_ || state_ != RuntimeCompositionState::kLost) return false;
  const auto& surface = runtime_->surface();
  if (!runtime_->rebindSurface() && !surface.available) return false;
  ++providerEpoch_;
  if (!transition(RuntimeCompositionState::kReady)) return false;
  refreshControlTarget();
  return true;
}
bool RuntimeComposition::rebindSurface(const render::SurfaceMetrics& metrics) noexcept {
  if (std::this_thread::get_id() != ownerThread_ || state_ != RuntimeCompositionState::kLost ||
      !runtime_->rebindSurface(metrics)) return false;
  ++providerEpoch_;
  if (!transition(RuntimeCompositionState::kReady)) return false;
  refreshControlTarget();
  return true;
}
bool RuntimeComposition::adoptPreparedSurface(
    std::uint64_t expectedGeneration, const render::SurfaceMetrics& metrics) noexcept {
  if (std::this_thread::get_id() != ownerThread_ ||
      state_ != RuntimeCompositionState::kLost || expectedGeneration == 0U) return false;
  const auto& surface = runtime_->surface();
  if (!surface.available || surface.generation != expectedGeneration ||
      surface.width != metrics.physicalWidth || surface.height != metrics.physicalHeight) {
    return false;
  }
  // The preview registry is independent from the canonical registry. Rebind
  // it only as part of this same owner-side commit; it must not advance the
  // canonical generation.
  if (!runtime_->adoptPreparedSurface(metrics)) return false;
  ++providerEpoch_;
  if (!transition(RuntimeCompositionState::kReady)) return false;
  refreshControlTarget();
  return true;
}
bool RuntimeComposition::stop() noexcept { if (std::this_thread::get_id() != ownerThread_ || (state_ != RuntimeCompositionState::kReady && state_ != RuntimeCompositionState::kLost)) return false; resources_.invalidate(); notifications_.clear(); runtime_->cancelAllPointers(); runtime_->close(); return transition(RuntimeCompositionState::kStopping) && transition(RuntimeCompositionState::kQuiescent); }
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
  out.metricsGeneration = surface.generation;
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
  if (std::this_thread::get_id() != ownerThread_) return false;
  auto* provider = runtime_->activeSurfaceProvider();
  return provider != nullptr &&
      provider->readbackRgba(destination).code == render::BackendSubmissionCode::kAccepted;
}
bool RuntimeComposition::readPreviewRgba(std::span<std::uint8_t> destination) noexcept {
  if (std::this_thread::get_id() != ownerThread_) return false;
  auto* provider = runtime_->previewSurfaceProvider();
  return provider != nullptr &&
      provider->readbackRgba(destination).code == render::BackendSubmissionCode::kAccepted;
}
bool RuntimeComposition::canonicalProviderAvailable() const noexcept {
  return runtime_->activeSurfaceProvider() != nullptr;
}
bool RuntimeComposition::previewProviderAvailable() const noexcept {
  return runtime_->previewSurfaceProvider() != nullptr;
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
