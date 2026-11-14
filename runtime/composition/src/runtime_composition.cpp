#include "canvas/foundation/internal/g5_observation.hpp"
#include "canvas/foundation/internal/frame_timing.hpp"
namespace g5 = canvas::foundation::internal::g5;
#include "canvas/runtime/internal/runtime_composition_core.hpp"
#include "canvas/runtime/canvas_runtime_facade.hpp"
#include "canvas/runtime/internal/multi_selection_port.hpp"
#include "canvas/render/image_resource.hpp"
#include "canvas/text/text_layout.hpp"

#include <cstddef>
#include <cstdint>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <limits>
#include <random>
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

RuntimeCompositionCore::RuntimeCompositionCore(render::ViewId viewId)
    : runtime_(std::make_unique<interaction::CanvasEditingRuntime>(viewId)),
      provider_(std::make_unique<MemoryResourceProvider>()),
      textLayoutService_(std::make_unique<canvas::text::RichTextLayoutService>(
          *provider_, canvas::text::LayoutContext{128.0F, 1U})),
      ownerThread_(std::this_thread::get_id()), viewId_(viewId),
      runtimeInstanceId_(allocateRuntimeInstanceId()) {
  runtime_->registerDisplayWorkOwner(runtimeInstanceId_);
  runtime_->bindResourceManifest(&resources_.manifest);
#if defined(CANVAS_RENDER_HAS_SKIA)
  imageResolver_ = std::make_unique<canvas::render::ImageResourceResolver>(*provider_);
#endif
  controlFacade_ = std::make_unique<CanvasRuntimeFacade>(
      *runtime_, CanvasTargetKey{1U, viewId.value(), 1U, 1U},runtimeInstanceId_, this);
  multiSelectionPort_ = std::make_unique<MultiSelectionPort>(*this);
#if defined(CANVAS_RENDER_HAS_SKIA)
  runtime_->bindResourceServices(imageResolver_.get(), textLayoutService_.get());
#else
  runtime_->bindResourceServices(nullptr, textLayoutService_.get());
#endif
}
void RuntimeCompositionCore::refreshControlTarget() noexcept {
  if (controlFacade_ == nullptr) return;
  const auto surface = runtime_->surface();
  controlFacade_->setTarget(CanvasTargetKey{
      resources_.lifetimeGeneration,
      viewId_.value(),
      surface.metricsGeneration == 0U ? 1U : surface.metricsGeneration,
      attachmentEpoch_});
}
bool RuntimeCompositionCore::canAdvanceSurfaceBinding() const noexcept {
  constexpr auto limit = std::numeric_limits<std::uint64_t>::max() - 1U;
  return attachmentEpoch_ < limit && providerEpoch_ < limit &&
      resources_.lifetimeGeneration < limit;
}
void RuntimeCompositionCore::surfaceBindingChanged() noexcept {
  if (controlFacade_ != nullptr) controlFacade_->discardImageCrop();
  if (attachmentEpoch_ != std::numeric_limits<std::uint64_t>::max()) ++attachmentEpoch_;
  if (providerEpoch_ != std::numeric_limits<std::uint64_t>::max()) ++providerEpoch_;
  resources_.invalidate();
  notifications_.clear();
  resourceReceipts_.clear();
  retirePreviewCapture();
  pendingCanonicalWork_.reset();
  refreshControlTarget();
}
RuntimeCompositionCore::~RuntimeCompositionCore() { (void)close(); }
RuntimeFacade& RuntimeCompositionCore::controlFacade() noexcept { return *controlFacade_; }
const RuntimeFacade& RuntimeCompositionCore::controlFacade() const noexcept { return *controlFacade_; }
MultiSelectionPort& RuntimeCompositionCore::multiSelectionPort() noexcept { return *multiSelectionPort_; }
bool RuntimeCompositionCore::transition(RuntimeCompositionState next) noexcept {
  if (state_ == RuntimeCompositionState::kClosed) return false;
  state_ = next; return true;
}
bool RuntimeCompositionCore::canMutate() const noexcept {
  return state_ == RuntimeCompositionState::kReady && !surfaceTransitionActive_;
}
bool RuntimeCompositionCore::open() noexcept {
  if (std::this_thread::get_id() != ownerThread_ || runtimeInstanceId_ == 0U ||
      state_ != RuntimeCompositionState::kCreated) return false;
  try {
    // The ephemeral frame instance counter and a restored document's ordinal
    // cannot identify new canonical operations across processes/WASM modules.
    // The supported Windows/WASM toolchains use OS/Web Crypto entropy here.
    // Never fall back to time, a process counter, or a restored ordinal.
    std::random_device entropy;
    foundation::ObjectId origin{};
    for (std::size_t index = 0; index < origin.bytes.size(); index += 4) {
      const auto word = entropy();
      for (std::size_t byte = 0; byte < 4; ++byte)
        origin.bytes[index + byte] = static_cast<std::uint8_t>(word >> (byte * 8));
    }
    if (!runtime_->setInitialOperationNamespace(origin)) return false;
    return transition(RuntimeCompositionState::kBound);
  } catch (...) { return false; }
}
bool RuntimeCompositionCore::openDocument(semantic::DocumentId id) noexcept {
  if (std::this_thread::get_id() != ownerThread_ || runtimeInstanceId_ == 0U ||
      state_ != RuntimeCompositionState::kCreated || id.isZero() ||
      !runtime_->setInitialDocumentId(id)) return false;
  return open();
}
semantic::DocumentId RuntimeCompositionCore::documentId() const noexcept {
  return runtime_->documentId();
}
bool RuntimeCompositionCore::setLocalOperationSink(interaction::LocalOperationSink* sink) noexcept {
  if (std::this_thread::get_id() != ownerThread_ || state_ == RuntimeCompositionState::kCreated) return false;
  runtime_->setLocalOperationSink(sink); return true;
}
semantic::CanonicalCommitStamp RuntimeCompositionCore::canonicalCommitFence() const noexcept {
  return runtime_->canonicalCommitFence();
}
DocumentSnapshotResult RuntimeCompositionCore::captureDocumentSnapshot(const DocumentSnapshotRequest& request) noexcept {
  DocumentSnapshotResult result{};
  if (std::this_thread::get_id() != ownerThread_) { result.status = DocumentSnapshotStatus::kWrongThread; return result; }
  if (!canMutate()) { result.status = DocumentSnapshotStatus::kNotReady; return result; }
  if (!runtime_->snapshotCaptureReady()) {
    result.status = DocumentSnapshotStatus::kBusy; return result;
  }
  result.semanticGeneration = runtime_->semanticGeneration().value();
  if (request.expectedSemanticGeneration != result.semanticGeneration) { result.status = DocumentSnapshotStatus::kStale; return result; }
  try {
    if (controlFacade_->readTextEditor().phase != TextPhase::kClosed) {
      result.status = DocumentSnapshotStatus::kBusy; return result;
    }
    semantic::SemanticSnapshot snapshot{};
    snapshot.document_id = runtime_->documentId(); snapshot.schema_version = 3;
    snapshot.objects = runtime_->snapshotObjects(); snapshot.manifest = resources_.manifest;
    snapshot.background = runtime_->documentBackground();
    snapshot.manifest_present = true; snapshot.document_revision = request.documentRevision;
    snapshot.recovery_frontier = request.recoveryFrontier;
    snapshot.capability_requirements = request.capabilityRequirements; snapshot.digest_algorithm = 1;
    if (snapshot.capability_requirements.empty() && !semantic::SnapshotCodec::populateRequirements(snapshot)) {
      result.status = DocumentSnapshotStatus::kInvalidCheckpoint; return result;
    }
    constexpr auto capability = semantic::SnapshotCapability::kAllWithCompactStrokeV1;
    if (!semantic::SnapshotCodec::bindDigests(snapshot, capability)) {
      result.status = DocumentSnapshotStatus::kInvalidCheckpoint; return result;
    }
    auto encoded = semantic::SnapshotCodec::encode(snapshot, capability);
    if (!encoded.ok()) { result.status = DocumentSnapshotStatus::kInvalidCheckpoint; return result; }
    result.bytes = std::move(encoded.bytes); result.status = DocumentSnapshotStatus::kCaptured;
    return result;
  } catch (...) { result.status = DocumentSnapshotStatus::kFailed; return result; }
}
DocumentRestoreStatus RuntimeCompositionCore::restoreSnapshotBeforeSurface(const semantic::SemanticSnapshot& snapshot,
    const semantic::SnapshotBootstrapper::CheckpointBinding& binding, std::uint64_t resourceBinding) noexcept {
  if (std::this_thread::get_id() != ownerThread_ || state_ != RuntimeCompositionState::kBound ||
      !bindManifest(snapshot.manifest, resourceBinding)) return DocumentRestoreStatus::kInvalidSnapshot;
  const auto outcome = runtime_->restoreSnapshotBeforeSurface(snapshot, binding, resources_.manifest);
  if (outcome == interaction::SnapshotRestoreOutcome::kInvalid) return DocumentRestoreStatus::kInvalidSnapshot;
  if (outcome == interaction::SnapshotRestoreOutcome::kSceneFailed) return DocumentRestoreStatus::kSceneFailed;
  return DocumentRestoreStatus::kRestored;
}
bool RuntimeCompositionCore::bindSurface(std::uint32_t width, std::uint32_t height) noexcept {
  return bindSurface(render::SurfaceMetrics{static_cast<float>(width),
                                             static_cast<float>(height), width,
                                             height, 1.0F, 1.0F});
}
bool RuntimeCompositionCore::bindSurface(const render::SurfaceMetrics& metrics) noexcept {
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
bool RuntimeCompositionCore::resizeSurface(std::uint32_t width, std::uint32_t height) noexcept {
  return resizeSurface(render::SurfaceMetrics{static_cast<float>(width),
                                               static_cast<float>(height), width,
                                               height, 1.0F, 1.0F});
}
bool RuntimeCompositionCore::resizeSurface(const render::SurfaceMetrics& metrics) noexcept {
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
  runtime_->serviceResourceRetirements(render::ResourceServiceMode::kBeginSurfaceTransition);
  const bool resized = runtime_->resizeSurface(metrics);
  runtime_->serviceResourceRetirements(render::ResourceServiceMode::kEndSurfaceTransition);
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
bool RuntimeCompositionCore::registerSurfaceProvider(
    std::string profileId,
    std::unique_ptr<render::SkiaSurfaceProvider> provider) noexcept {
  if (std::this_thread::get_id() != ownerThread_ || surfaceTransitionActive_ ||
      (state_ != RuntimeCompositionState::kBound &&
       state_ != RuntimeCompositionState::kLost && !canMutate()) ||
      provider == nullptr || profileId.empty()) return false;
  return runtime_->stageSurfaceProvider(std::move(profileId), std::move(provider), false);
}
bool RuntimeCompositionCore::registerPreviewSurfaceProvider(
    std::string profileId,
    std::unique_ptr<render::SkiaSurfaceProvider> provider) noexcept {
  if (std::this_thread::get_id() != ownerThread_ || surfaceTransitionActive_ ||
      (state_ != RuntimeCompositionState::kBound &&
       state_ != RuntimeCompositionState::kLost && !canMutate()) ||
      provider == nullptr || profileId.empty()) return false;
  return runtime_->stageSurfaceProvider(std::move(profileId), std::move(provider), true);
}
bool RuntimeCompositionCore::refreshManifestDerivedState() noexcept {
  runtime_->bindResourceManifest(&resources_.manifest);
  runtime_->refreshResourceManifestServices();
  return true;
}
bool RuntimeCompositionCore::bindManifest(const semantic::ResourceManifest& manifest, std::uint64_t binding) noexcept {
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
  if (controlFacade_) controlFacade_->discardImageCrop();
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
bool RuntimeCompositionCore::registerSource(input::InputSourceId source) noexcept {
  if (std::this_thread::get_id() != ownerThread_) return false;
  return canMutate() &&
      runtime_->registerInputSource(source, "composition");
}
bool RuntimeCompositionCore::unregisterSource(input::InputSourceId source,
                                           std::uint64_t sequence,
                                           std::uint64_t timestampNs) noexcept {
  if (std::this_thread::get_id() != ownerThread_ || surfaceTransitionActive_ || !canMutate()) return false;
  if (controlFacade_ && controlFacade_->imageCrop_ && controlFacade_->imageCrop_->pointer.source == source)
    controlFacade_->discardImageCrop();
  return runtime_->unregisterInputSource(source, sequence, timestampNs);
}
bool RuntimeCompositionCore::cancelAllPointers() noexcept {
  if (std::this_thread::get_id() != ownerThread_ || !canMutate()) return false;
  runtime_->cancelAllPointers();
  return true;
}
bool RuntimeCompositionCore::submit(const input::PlatformPointerBatch& batch) noexcept {
  G5_SCOPE(g5Input,g5::Tag::input,g5::Purpose::none);

#if defined(AXIOM_G5_UR0_DIAGNOSTICS)
  g5::OwnerScope g5Owner;
  if(runtime_&&std::this_thread::get_id()==ownerThread_){g5Owner.bind({runtimeInstanceId_,resources_.lifetimeGeneration,attachmentEpoch_,g5::word(runtime_->documentId(),0),g5::word(runtime_->documentId(),1),runtime_->canonicalCommitFence().runtime_epoch.value(),resources_.manifest.manifest_revision,viewId_.value()});}
#endif


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
bool RuntimeCompositionCore::hoverSelectionAtViewPoint(float x, float y) noexcept {
  if (std::this_thread::get_id() != ownerThread_ || !canMutate()) return false;
  return runtime_->hoverSelectionAtViewPoint(x, y);
}
std::optional<input::PointerKey> RuntimeCompositionCore::platformKey(
    input::InputSourceId source, input::PointerId pointer) const noexcept {
  if (std::this_thread::get_id() != ownerThread_ || !runtime_) return std::nullopt;
  return runtime_->platformKey(source, pointer);
}
bool RuntimeCompositionCore::cancelPlatformPointer(const input::PointerKey& key,
    std::uint64_t sequence, std::uint64_t timestampNs) noexcept {
  if (std::this_thread::get_id() != ownerThread_ || surfaceTransitionActive_ || !canMutate()) return false;
  if (controlFacade_ && controlFacade_->imageCrop_ && controlFacade_->imageCrop_->pointer == key)
    controlFacade_->discardImageCrop();
  return runtime_->cancelPlatformPointer(key, sequence, timestampNs);
}
std::size_t RuntimeCompositionCore::semanticObjectCount() const noexcept {
  return std::this_thread::get_id() == ownerThread_ && runtime_
      ? runtime_->semanticObjectCount() : 0U;
}
semantic::ApplyResult RuntimeCompositionCore::applyExternalOperation(
    const semantic::Operation& operation, semantic::ApplySource source) noexcept {
  if (runtime_->cpuPendingBusy() || !canMutate() || std::this_thread::get_id() != ownerThread_) return {};
  return runtime_->applyExternalOperation(operation, source);
}
bool RuntimeCompositionCore::notifyResource(foundation::ObjectId id) noexcept {
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
bool RuntimeCompositionCore::publishResource(foundation::ObjectId id, std::vector<std::uint8_t> bytes) noexcept {
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
bool RuntimeCompositionCore::publishResource(const ResourceBindingToken& token,
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
bool RuntimeCompositionCore::ownerPump() noexcept {
#if defined(AXIOM_G5_UR0_DIAGNOSTICS)
  g5::OwnerScope g5Owner;
  if(runtime_&&std::this_thread::get_id()==ownerThread_){g5Owner.bind({runtimeInstanceId_,resources_.lifetimeGeneration,attachmentEpoch_,g5::word(runtime_->documentId(),0),g5::word(runtime_->documentId(),1),runtime_->canonicalCommitFence().runtime_epoch.value(),resources_.manifest.manifest_revision,viewId_.value()});}
#endif

  if (std::this_thread::get_id() != ownerThread_) { lastResourceError_ = ResourcePortError::kWrongThread; return false; }
  if (!canMutate() && state_!=RuntimeCompositionState::kLost) { lastResourceError_ = ResourcePortError::kNotReady; return false; }
  ++ownerPumpCount_;
  lastOwnerControlCount_ = 0;
  if (controlFacade_ != nullptr) {
    lastOwnerControlCount_ = controlFacade_->process(ownerPumpCount_);
  }
  if(canMutate() && !runtime_->serviceConnectorRouting())return false;
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
bool RuntimeCompositionCore::requestCanonicalFrame(std::uint64_t frameId) noexcept {
#if defined(AXIOM_G5_UR0_DIAGNOSTICS)
  g5::OwnerScope g5Owner;
  if(runtime_&&std::this_thread::get_id()==ownerThread_){g5Owner.bind({runtimeInstanceId_,resources_.lifetimeGeneration,attachmentEpoch_,g5::word(runtime_->documentId(),0),g5::word(runtime_->documentId(),1),runtime_->canonicalCommitFence().runtime_epoch.value(),resources_.manifest.manifest_revision,viewId_.value()});}
#endif

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
  return G5_CALL(g5::Tag::canonical,g5::Purpose::canonical,runtime_->presentCanonicalFrame(frameId, 0.0, false)) && captureCanonicalWork();
}
bool RuntimeCompositionCore::resourceRetirementPending() const noexcept {
  return runtime_ && runtime_->resourceRetirementPending();
}

void RuntimeCompositionCore::suspendCpuPendingExecutor()noexcept{if(std::this_thread::get_id()==ownerThread_&&runtime_)runtime_->suspendCpuPendingExecutor();}
void RuntimeCompositionCore::failCpuPendingExecutor()noexcept{if(std::this_thread::get_id()==ownerThread_&&runtime_)runtime_->failCpuPendingExecutor();}
bool RuntimeCompositionCore::bindCpuPendingExecutor(std::uint64_t e)noexcept{
 if(std::this_thread::get_id()!=ownerThread_||!canMutate()||!runtime_)return false;
 return runtime_->bindCpuPendingExecutor(cpuWorkerBasis(),e);
}
std::array<std::uint64_t,4> RuntimeCompositionCore::cpuWorkerBasis()const noexcept{
 if(runtime_){const auto pending=runtime_->pendingLocalValue();if(pending.owner[0]){std::array<std::uint64_t,4> basis{pending.owner[0],pending.owner[1],0,0};const auto doc=runtime_->documentId().value();for(unsigned i=0;i<16;++i)basis[2+i/8]|=std::uint64_t(doc.bytes[i])<<((i%8)*8);return basis;}}
 std::array<std::uint64_t,4> basis{runtimeInstanceId_,resources_.lifetimeGeneration,0,0};if(runtime_){const auto doc=runtime_->documentId().value();for(unsigned i=0;i<16;++i)basis[2+i/8]|=std::uint64_t(doc.bytes[i])<<((i%8)*8);}return basis;
}
std::vector<std::uint8_t> RuntimeCompositionCore::takeCpuPendingRequest()noexcept{
 return std::this_thread::get_id()==ownerThread_&&runtime_?runtime_->takeCpuPendingRequest():std::vector<std::uint8_t>{};
}
bool RuntimeCompositionCore::queueCpuPendingCompletion(std::array<std::uint64_t,8> j,std::span<const std::uint8_t> b,bool ok)noexcept{
 return std::this_thread::get_id()==ownerThread_&&runtime_&&runtime_->queueCpuPendingCompletion(j,b,ok);
}
interaction::PendingLocalValue RuntimeCompositionCore::pendingLocalValue()const{return runtime_?runtime_->pendingLocalValue():interaction::PendingLocalValue{};}
interaction::CpuPendingEraseFacts RuntimeCompositionCore::cpuPendingFacts()const noexcept{return runtime_?runtime_->cpuPendingFacts():interaction::CpuPendingEraseFacts{};}
std::uint64_t RuntimeCompositionCore::reserveCpuWorkerGrant(std::uint64_t e,std::size_t b)noexcept{
 if(std::this_thread::get_id()!=ownerThread_||!canMutate()||!runtime_)return 0;
 return runtime_->reserveCpuWorkerGrant(e,b);
}
bool RuntimeCompositionCore::releaseCpuWorkerGrant(std::uint64_t e,std::uint64_t t)noexcept{
 return std::this_thread::get_id()==ownerThread_&&runtime_&&runtime_->releaseCpuWorkerGrant(e,t);
}
std::size_t RuntimeCompositionCore::cpuWorkerGrantBytes()const noexcept{return runtime_?runtime_->cpuWorkerGrantBytes():0;}
bool RuntimeCompositionCore::serviceFrameOpportunity(
    bool forceCanonical, RuntimeFrameOpportunity& out) noexcept {

#if defined(AXIOM_G5_UR0_DIAGNOSTICS)
  g5::OwnerScope g5Owner;
  if(runtime_&&std::this_thread::get_id()==ownerThread_){g5Owner.bind({runtimeInstanceId_,resources_.lifetimeGeneration,attachmentEpoch_,g5::word(runtime_->documentId(),0),g5::word(runtime_->documentId(),1),runtime_->canonicalCommitFence().runtime_epoch.value(),resources_.manifest.manifest_revision,viewId_.value()});}
#endif


  out = {};
  if (std::this_thread::get_id() != ownerThread_ || !canMutate()) return false;
  AXIOM_FRAME_TIMING_BEGIN(false,viewId_.value(),nextCanonicalWorkId_,runtime_->semanticGeneration().value(),runtime_->sceneRevision(),runtime_->cameraGeneration(),runtime_->surface().generation,runtime_->surface().metricsGeneration,"owner");
  // A capture may be superseded by input, resize, restore, or cancellation
  // before the platform reports its presentation result.  Invalidate that
  // obsolete work at the next owner opportunity so it cannot reserve the
  // single in-flight slot indefinitely.
  if (pendingPreviewCapture_.has_value()) {
    if (!pendingPreviewIdentity_ ||
        previewWorkState(*pendingPreviewIdentity_) == RuntimePreviewWorkState::kSuperseded)
      retirePreviewCapture();
  }
  // Consume controls and resource notifications first. Their application may
  // create a new canonical demand during this same frame opportunity.
  {AXIOM_FRAME_TIMING_SCOPE(ownerPumpTiming,owner_pump);if (!ownerPump()) return false;}
  AXIOM_IF_EVENT("nc_route point=before_pending semantic=%llu scene=%llu camera=%llu partial=%u transient_dirty=%u canonical_dirty=%u",(unsigned long long)runtime_->semanticGeneration().value(),(unsigned long long)runtime_->sceneRevision(),(unsigned long long)runtime_->cameraGeneration(),unsigned(runtime_->partialEraseFeedbackActive()),unsigned(runtime_->transientOverlayDirty()),unsigned(runtime_->canonicalPresentationDirty()));
  {AXIOM_FRAME_TIMING_SCOPE(pendingTiming,pending_owner);runtime_->serviceCpuPendingErases();}
  try{if(auto image=runtime_->pendingHeadImage()){
    bool applied=false;try{applied=controlFacade_->importImage(image->second).state==ImageCommandState::kApplied;}catch(...){}
    runtime_->completePendingImage(image->first,applied);
  }}catch(...){runtime_->completePendingImage(runtime_->cpuPendingFacts().head,false);}
  {AXIOM_FRAME_TIMING_SCOPE(resourceTiming,resource_service);runtime_->serviceResourceRetirements();}
  runtime_->updateTextCaretPresentation(std::chrono::steady_clock::now());
  runtime_->advanceLaserPresentation(static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count()));
  const bool shouldCanonical = forceCanonical || runtime_->canonicalPresentationDirty();
  if (nextCanonicalWorkId_ == std::numeric_limits<std::uint64_t>::max()) return false;
  const bool editingTransient=!forceCanonical && ((runtime_->platformPresentationDeferred() && (runtime_->creationActive() ||
      runtime_->selectionTransformActive() || runtime_->regionSelectionActive())) || runtime_->objectEraseFeedbackActive() || runtime_->partialEraseFeedbackActive());
  AXIOM_IF_EVENT("nc_route point=after_pending semantic=%llu scene=%llu camera=%llu transient=%u transient_dirty=%u canonical_dirty=%u",(unsigned long long)runtime_->semanticGeneration().value(),(unsigned long long)runtime_->sceneRevision(),(unsigned long long)runtime_->cameraGeneration(),unsigned(editingTransient),unsigned(runtime_->transientOverlayDirty()),unsigned(shouldCanonical));
  bool transientRendered=false;
  {AXIOM_FRAME_TIMING_SCOPE(transientTiming,transient_frame);transientRendered=editingTransient && runtime_->transientOverlayDirty() && runtime_->retryTransientOverlayFrame();}
  if(pendingPreviewCapture_&&pendingPreviewCapture_->displayRevision!=runtime_->previewDisplayRevision()){
    retirePreviewCapture();
  }
  if(shouldCanonical && !forceCanonical && !editingTransient){
    AXIOM_FRAME_TIMING_SCOPE(navTiming,navigation_frame);
    out.navigationPending=runtime_->serviceNavigationPresentation(
        std::chrono::steady_clock::now(),out.navigationPresented);
  }
  // Selection chrome is a canonical overlay, but the first contact of a
  // possible pinch is still under AutoIntent arbitration. Do not spend this
  // frame opportunity on a full scene render before the second contact can
  // claim the viewport gesture; the eventual single-pointer Up still
  // submits the selection frame normally.
  const bool awaitingViewportClaim = runtime_->selectionOverlay() != nullptr &&
      runtime_->pendingViewportGesture();
  if (shouldCanonical && !out.navigationPending && !editingTransient &&
      !awaitingViewportClaim) {
    AXIOM_FRAME_TIMING_SCOPE(canonicalTiming,canonical_frame);
    const auto frameId = runtime_->nextCanonicalFrameId();
    if (frameId == 0U) return false;
    out.canonicalRequested = G5_CALL(g5::Tag::canonical,g5::Purpose::canonical,runtime_->presentCanonicalFrame(frameId, 0.0, false));
    if (!out.canonicalRequested && !runtime_->canonicalPresentationRetryable()) return false;
  }
  if (!captureCanonicalWork()) return false;
  out.previewActive = runtime_->previewActive();
  out.previewDirty = runtime_->previewRenderState().dirty||runtime_->previewDisplayDirty();
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
            pendingPreviewCapture_->surfaceGeneration, runtime_->surface().metricsGeneration,
            pendingPreviewCapture_->displayRevision, attachmentEpoch_};
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
  // Navigation feedback may reuse the most recently submitted canonical
  // frame as a disposable visual source while its platform receipt is still
  // pending. Do not expose that same canonical work to the platform fence a
  // second time; the original work remains retained in RuntimeCompositionCore
  // and will be completed by receivePlatformPresented().
  if (out.navigationPending || out.navigationPresented) {
    out.canonicalWork.reset();
  }
  out.needsPaint = runtime_->cpuPendingFacts().reserved!=0 || out.canonicalRequested || out.navigationPresented || out.previewDirty || transientRendered ||
      (editingTransient && runtime_->transientOverlayDirty()) ||
      out.pendingCanonicalHandoff != 0U || out.canonicalFrame.has_value() || runtime_->canonicalPresentationDirty();
  AXIOM_IF_EVENT("nc_route point=outcome semantic=%llu scene=%llu camera=%llu transient=%u transient_rendered=%u transient_dirty=%u nav_pending=%u nav_presented=%u canonical_requested=%u canonical_retry=%u canonical_work=%u needs_paint=%u",(unsigned long long)runtime_->semanticGeneration().value(),(unsigned long long)runtime_->sceneRevision(),(unsigned long long)runtime_->cameraGeneration(),unsigned(editingTransient),unsigned(transientRendered),unsigned(runtime_->transientOverlayDirty()),unsigned(out.navigationPending),unsigned(out.navigationPresented),unsigned(out.canonicalRequested),unsigned(runtime_->canonicalPresentationRetryable()),unsigned(out.canonicalWork.has_value()),unsigned(out.needsPaint));
  AXIOM_FRAME_TIMING_SET(commit_ordinal,runtime_->canonicalCommitFence().ordinal.value());
  AXIOM_FRAME_TIMING_SET(after_generation,runtime_->semanticGeneration().value());
  AXIOM_FRAME_TIMING_SET(after_scene,runtime_->sceneRevision());
  AXIOM_FRAME_TIMING_OUTCOME(out.needsPaint?"pending":"settled");
  return true;
}
void RuntimeCompositionCore::retirePreviewCapture() noexcept {
  if (pendingPreviewIdentity_) supersededPreviewIdentity_ = pendingPreviewIdentity_;
  pendingPreviewCapture_.reset();
  pendingPreviewPresentCount_ = 0U;
  pendingPreviewWorkId_ = 0U;
  pendingPreviewIdentity_.reset();
}
RuntimePreviewWorkState RuntimeCompositionCore::previewWorkState(
    const RuntimeFrameOpportunity::PreviewWork& work) const noexcept {
  if (std::this_thread::get_id() != ownerThread_ ||
      work.runtimeInstanceId != runtimeInstanceId_ || work.viewId != viewId_.value() ||
      work.workId == 0U) return RuntimePreviewWorkState::kInvalid;
  if (supersededPreviewIdentity_ && work == *supersededPreviewIdentity_)
    return RuntimePreviewWorkState::kSuperseded;
  if (!pendingPreviewIdentity_ || work != *pendingPreviewIdentity_ ||
      !pendingPreviewCapture_) return RuntimePreviewWorkState::kInvalid;
  const auto& capture = *pendingPreviewCapture_;
  const bool current = canMutate() && runtime_->previewActive() &&
      work.lifetimeGeneration == resources_.lifetimeGeneration &&
      work.attachmentEpoch == attachmentEpoch_ &&
      work.workId == pendingPreviewWorkId_ &&
      work.contentRevision == capture.contentRevision &&
      work.contentRevision == runtime_->previewRenderState().contentRevision &&
      work.displayRevision == capture.displayRevision &&
      work.displayRevision == runtime_->previewDisplayRevision() &&
      work.surfaceGeneration == capture.surfaceGeneration &&
      work.surfaceGeneration == runtime_->previewSurfaceGeneration() &&
      work.metricsGeneration == runtime_->surface().metricsGeneration;
  return current ? RuntimePreviewWorkState::kCurrent : RuntimePreviewWorkState::kSuperseded;
}
bool RuntimeCompositionCore::finishFrameOpportunity(
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
  if (previewWorkState(*completion.previewWork) != RuntimePreviewWorkState::kCurrent) {
    retirePreviewCapture();
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
bool RuntimeCompositionCore::captureCanonicalWork() noexcept {
  G5_SCOPE(g5Capture,g5::Tag::canonical_basis,g5::Purpose::canonical);
  const auto frame = runtime_->lastSubmittedCanonicalFrame();
  if (!frame) { pendingCanonicalWork_.reset(); return true; }
  if (lastSubmittedCanonicalWork()) return true;
  if (runtimeInstanceId_ == 0U ||
      nextCanonicalWorkId_ == std::numeric_limits<std::uint64_t>::max()) return false;
  pendingCanonicalWork_ = CanonicalWork{runtimeInstanceId_, resources_.lifetimeGeneration,
      attachmentEpoch_, nextCanonicalWorkId_++, *frame};
  if(!runtime_->bindDisplayWork({runtimeInstanceId_,resources_.lifetimeGeneration,attachmentEpoch_,pendingCanonicalWork_->workId},*frame))return false;
  G5_OBSERVE(g5::Kind::Basis,g5::Tag::canonical_basis,true,pendingCanonicalWork_->workId,runtimeInstanceId_,resources_.lifetimeGeneration,attachmentEpoch_,g5::word(runtime_->documentId(),0),g5::word(runtime_->documentId(),1),runtime_->canonicalCommitFence().runtime_epoch.value(),runtime_->canonicalCommitFence().ordinal.value(),resources_.manifest.manifest_revision,runtime_->sceneRevision(),runtime_->cameraGeneration());
  G5_OBSERVE(g5::Kind::Submit,g5::Tag::canonical_receipt,true,pendingCanonicalWork_->workId,frame->viewId.value(),frame->frameId.value(),frame->documentRevision,frame->sceneGeneration,frame->surfaceGeneration.value(),frame->metricsGeneration.value());
  return true;
}
std::optional<CanonicalWork> RuntimeCompositionCore::lastSubmittedCanonicalWork() const noexcept {
  if (!canMutate() || !pendingCanonicalWork_ ||
      pendingCanonicalWork_->runtimeInstanceId != runtimeInstanceId_ ||
      pendingCanonicalWork_->lifetimeGeneration != resources_.lifetimeGeneration ||
      pendingCanonicalWork_->attachmentEpoch != attachmentEpoch_) return std::nullopt;
  const auto current = runtime_->lastSubmittedCanonicalFrame();
  if (!current || *current != pendingCanonicalWork_->frame) return std::nullopt;
  return pendingCanonicalWork_;
}
bool RuntimeCompositionCore::receivePlatformPresented(const CanonicalCompletion& completion) noexcept {
  G5_SCOPE(g5Completion,g5::Tag::canonical_receipt,g5::Purpose::input);
#if defined(AXIOM_G5_UR0_DIAGNOSTICS)
  g5::OwnerScope g5Owner;
  if(runtime_&&std::this_thread::get_id()==ownerThread_){g5Owner.bind({runtimeInstanceId_,resources_.lifetimeGeneration,attachmentEpoch_,g5::word(runtime_->documentId(),0),g5::word(runtime_->documentId(),1),runtime_->canonicalCommitFence().runtime_epoch.value(),resources_.manifest.manifest_revision,viewId_.value()});}
#endif

  G5_OBSERVE(g5::Kind::Complete,g5::Tag::canonical_receipt,true,0,completion.work.workId,completion.work.runtimeInstanceId,completion.work.lifetimeGeneration,completion.work.attachmentEpoch,completion.work.frame.viewId.value(),completion.work.frame.frameId.value(),completion.work.frame.documentRevision,completion.work.frame.sceneGeneration,completion.work.frame.surfaceGeneration.value(),completion.work.frame.metricsGeneration.value());

  if (std::this_thread::get_id() != ownerThread_ || !canMutate()) return false;
  const auto current = lastSubmittedCanonicalWork();
  if (!current || completion.work != *current) return false;
  const auto& frame = current->frame;
  const bool accepted = runtime_->receivePlatformPresented({frame.viewId, frame.frameId,
      frame.surfaceGeneration, frame.metricsGeneration, completion.outcome,
      completion.evidence, completion.presentedTimeNs});
  G5_OBSERVE(g5::Kind::Complete,g5::Tag::canonical_receipt,true,accepted?1ULL:2ULL,completion.work.workId,completion.work.runtimeInstanceId,completion.work.lifetimeGeneration,completion.work.attachmentEpoch,frame.viewId.value(),frame.frameId.value(),frame.documentRevision,frame.sceneGeneration,frame.surfaceGeneration.value(),frame.metricsGeneration.value());
  if (accepted) { runtime_->completeDisplayWork({current->runtimeInstanceId,current->lifetimeGeneration,current->attachmentEpoch,current->workId},frame);pendingCanonicalWork_.reset(); }
  return accepted;
}
bool RuntimeCompositionCore::loseSurface() noexcept {
  if (std::this_thread::get_id() != ownerThread_ ||
      state_ != RuntimeCompositionState::kReady) return false;
  runtime_->serviceResourceRetirements(render::ResourceServiceMode::kBeginSurfaceTransition);
  const bool lost=runtime_->loseSurface();
  runtime_->serviceResourceRetirements(render::ResourceServiceMode::kEndSurfaceTransition);
  if(!lost)return false;
  state_ = RuntimeCompositionState::kLost;
  surfaceBindingChanged();
  return true;
}
bool RuntimeCompositionCore::rebindSurface() noexcept {
  const auto surface = surfaceSnapshot();
  return rebindSurface(render::SurfaceMetrics{surface.logicalWidth, surface.logicalHeight,
      surface.width, surface.height, surface.devicePixelRatio, surface.displayScale});
}
bool RuntimeCompositionCore::rebindSurface(const render::SurfaceMetrics& metrics) noexcept {
  if (std::this_thread::get_id() != ownerThread_ || surfaceTransitionActive_ ||
      state_ != RuntimeCompositionState::kLost || !canAdvanceSurfaceBinding()) return false;
  surfaceTransitionActive_ = true;
  runtime_->serviceResourceRetirements(render::ResourceServiceMode::kBeginSurfaceTransition);
  const bool rebound = runtime_->rebindSurface(metrics);
  runtime_->serviceResourceRetirements(render::ResourceServiceMode::kEndSurfaceTransition);
  surfaceTransitionActive_ = false;
  if (!rebound || state_ != RuntimeCompositionState::kLost) return false;
  state_ = RuntimeCompositionState::kReady;
  surfaceBindingChanged();
  return true;
}
bool RuntimeCompositionCore::adoptPreparedSurface(
    std::uint64_t expectedGeneration, const render::SurfaceMetrics& metrics) noexcept {
  if (expectedGeneration == 0U || expectedGeneration != runtime_->surface().generation) return false;
  // Expected identity is the last accepted binding, not a host-invented future
  // generation. Both prepared candidates are committed by the same path.
  return rebindSurface(metrics);
}
bool RuntimeCompositionCore::stop() noexcept {
  if (std::this_thread::get_id() != ownerThread_ ||
      (state_ != RuntimeCompositionState::kReady && state_ != RuntimeCompositionState::kLost)) return false;
  if (controlFacade_) {controlFacade_->discardImageCrop();controlFacade_->setTarget({});}
  resources_.invalidate();
  notifications_.clear();
  retirePreviewCapture();
  pendingCanonicalWork_.reset();
  runtime_->close();
  return transition(RuntimeCompositionState::kStopping) && transition(RuntimeCompositionState::kQuiescent);
}
bool RuntimeCompositionCore::detach() noexcept {
  if (std::this_thread::get_id() != ownerThread_ || state_ != RuntimeCompositionState::kQuiescent) return false;
  resources_.invalidate();
  notifications_.clear();
  return true;
}
bool RuntimeCompositionCore::close() noexcept { if (std::this_thread::get_id() != ownerThread_) return false; if(runtime_&&(runtime_->cpuPendingBusy()||runtime_->cpuWorkerGrantBytes()))return false; if (state_ == RuntimeCompositionState::kClosed) return true; if (controlFacade_ != nullptr) {controlFacade_->discardImageCrop();controlFacade_->setTarget({});} if (state_ == RuntimeCompositionState::kReady || state_ == RuntimeCompositionState::kLost) (void)stop(); else runtime_->close(); resources_.invalidate(); notifications_.clear(); state_ = RuntimeCompositionState::kClosed; return true; }
std::uint64_t RuntimeCompositionCore::canonicalPresentCount() const noexcept {
  const auto* provider = runtime_->activeSurfaceProvider();
  return provider == nullptr ? 0U : provider->presentCount();
}
RuntimeCompositionReceipt RuntimeCompositionCore::readback() const noexcept {
  const auto& runtime = *runtime_;
  return {state_, resources_.lifetimeGeneration, ownerPumpCount_, lastOwnerControlCount_, resources_.manifest.manifest_revision, runtime.sceneRevision(), runtime.semanticGeneration().value(), notifications_.size(), runtime.lastSubmittedCanonicalFrame().has_value(), runtime.canonicalPresentationDirty()};
}

RuntimeSurfaceSnapshot RuntimeCompositionCore::surfaceSnapshot() const noexcept {
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

std::uint64_t RuntimeCompositionCore::previewSurfaceGeneration() const noexcept {
  return runtime_->previewSurfaceGeneration();
}

render::RenderTargetBackend RuntimeCompositionCore::canonicalBackend() const noexcept {
  const auto* provider = const_cast<interaction::CanvasEditingRuntime&>(*runtime_)
      .activeSurfaceProvider();
  return provider == nullptr ? render::RenderTargetBackend::kRaster
                             : provider->describe().backend;
}

std::uint64_t RuntimeCompositionCore::nextCanonicalFrameId() const noexcept {
  return runtime_->nextCanonicalFrameId();
}
std::uint64_t RuntimeCompositionCore::canonicalContentSerial() const noexcept {
  return runtime_->canonicalFrameCount();
}
std::optional<render::RuntimeFrameIdentity>
RuntimeCompositionCore::lastSubmittedCanonicalFrame() const noexcept {
  return runtime_->lastSubmittedCanonicalFrame();
}
bool RuntimeCompositionCore::canonicalPresentationDirty() const noexcept {
  return runtime_->canonicalPresentationDirty();
}
bool RuntimeCompositionCore::previewActive() const noexcept { return runtime_->previewActive(); }
const render::SurfaceRenderState& RuntimeCompositionCore::previewRenderState() const noexcept {
  return runtime_->previewRenderState();
}
bool RuntimeCompositionCore::capturePreviewPresentation(
    interaction::PreviewPresentationCapture& capture) const noexcept {
  return const_cast<interaction::CanvasEditingRuntime&>(*runtime_)
      .capturePreviewPresentation(capture);
}
bool RuntimeCompositionCore::renderPreviewPresentation(
    const interaction::PreviewPresentationCapture& capture,
    std::uint64_t* presentCount) noexcept {
  if (std::this_thread::get_id() != ownerThread_) return false;
  return runtime_->renderPreviewPresentation(capture, presentCount);
}
bool RuntimeCompositionCore::acknowledgePreviewPresentation(
    const interaction::PreviewPresentationCapture& capture,
    std::uint64_t presentCount) noexcept {
  if (std::this_thread::get_id() != ownerThread_) return false;
  return runtime_->acknowledgePreviewPresentation(capture, presentCount);
}
bool RuntimeCompositionCore::readCanonicalRgba(std::span<std::uint8_t> destination) noexcept {
  if (std::this_thread::get_id() != ownerThread_ || !canMutate()) return false;
  auto* provider = runtime_->activeSurfaceProvider();
  return provider != nullptr &&
      provider->readbackRgba(destination).code == render::BackendSubmissionCode::kAccepted;
}
bool RuntimeCompositionCore::readPreviewRgba(std::span<std::uint8_t> destination) noexcept {
  if (std::this_thread::get_id() != ownerThread_ || !canMutate()) return false;
  auto* provider = runtime_->previewSurfaceProvider();
  return provider != nullptr &&
      provider->readbackRgba(destination).code == render::BackendSubmissionCode::kAccepted;
}
bool RuntimeCompositionCore::canonicalProviderAvailable() const noexcept {
  return canMutate() && runtime_->surface().available && runtime_->activeSurfaceProvider() != nullptr;
}
bool RuntimeCompositionCore::previewProviderAvailable() const noexcept {
  return canMutate() && runtime_->surface().available && runtime_->previewSurfaceProvider() != nullptr;
}
bool RuntimeCompositionCore::setPlatformPresentationDeferred(bool deferred) noexcept {
  if (std::this_thread::get_id() != ownerThread_ || !canMutate()) return false;
  runtime_->setPlatformPresentationDeferred(deferred);
  return true;
}
bool RuntimeCompositionCore::applyViewportNavigation(
    const interaction::ViewportNavigationSample& sample) noexcept {
  if (std::this_thread::get_id() != ownerThread_ || surfaceTransitionActive_ ||
      !canMutate() || !runtime_->surface().available ||
      (sample.kind != interaction::ViewportNavigationKind::kWheelPan &&
       sample.kind != interaction::ViewportNavigationKind::kBrowserGesture) ||
      !std::isfinite(sample.deltaX) || !std::isfinite(sample.deltaY) ||
      !std::isfinite(sample.anchorX) || !std::isfinite(sample.anchorY) ||
      !std::isfinite(sample.scaleDelta) || sample.scaleDelta <= 0.0F) return false;
  return runtime_->applyViewportNavigation(sample);
}
std::optional<ResourcePortReceipt> RuntimeCompositionCore::resourceReceipt(
    foundation::ObjectId id) const noexcept {
  if (resources_.find(id) == nullptr) return std::nullopt;
  const auto it = resourceReceipts_.find(id);
  return it == resourceReceipts_.end() ? std::nullopt : std::optional<ResourcePortReceipt>(it->second);
}

std::optional<ResourceBindingToken> RuntimeCompositionCore::currentResourceToken(
    foundation::ObjectId id) const noexcept {
  if (std::this_thread::get_id() != ownerThread_) return std::nullopt;
  return resources_.token(id, providerEpoch_, nextRequestId_);
}
std::optional<std::vector<std::uint8_t>>
RuntimeCompositionCore::readResourceBytes(const ResourceBindingToken& token) const noexcept {
    if (std::this_thread::get_id() != ownerThread_ || !provider_)
        return std::nullopt;
    try {
        const auto expected = resources_.token(token.resourceId, providerEpoch_, token.requestId);
        if (!expected || *expected != token)
            return std::nullopt;
        const auto* entry = resources_.find(token.resourceId);
        const auto bytes = provider_->resolve(token.resourceId);
        if (!entry || !bytes.bytes || !validateResourceBytes(*entry, bytes))
            return std::nullopt;
        return *bytes.bytes;
    } catch (...) {
        return std::nullopt;
    }
}

bool RuntimeCompositionCore::imageResourceDecodeReady(
    foundation::ObjectId id) noexcept {
  if (std::this_thread::get_id() != ownerThread_ || provider_ == nullptr)
    return false;
#if defined(CANVAS_RENDER_HAS_SKIA)
  if (imageResolver_ == nullptr) return false;
  const auto decoded = imageResolver_->resolve(semantic::ResourceId{id});
  return decoded != nullptr && decoded->state == render::ImageResourceState::kReady;
#else
  (void)id;
  return true;
#endif
}
}  // namespace canvas::runtime
