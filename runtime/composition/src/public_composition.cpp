#include "canvas/runtime/runtime_composition.hpp"

#include "canvas/runtime/internal/runtime_composition_core.hpp"
#include "canvas/runtime/internal/runtime_platform_access.hpp"
#include <thread>

namespace canvas::runtime {

class RuntimeComposition::Impl final {
 public:
  class Sink final : public interaction::LocalOperationSink {
   public:
    RuntimeComposition::LocalOperationCallback callback = nullptr;
    RuntimeComposition::LocalOperationPrepare prepare = nullptr;
    RuntimeComposition::LocalOperationAbort abort = nullptr;
    RuntimeComposition::LocalOperationStatus readStatus = nullptr;
    void* context = nullptr;
    mutable bool active = false;
    bool preparePublication(std::span<const semantic::Operation> operations) noexcept override {
        if (active)
            return false;
        active = true;
        const bool result = !prepare || prepare(context, operations);
        active = false;
        return result;
    }
    void abortPreparedPublication() noexcept override {
        if (abort) {
            active = true;
            abort(context);
            active = false;
        }
    }
    void applied(const semantic::Operation& operation,
                 const semantic::CanonicalCommitRecord& record) noexcept override {
      if (!callback || active) return;
      active = true; callback(context, operation, record); active = false;
    }
    interaction::AxisOperationStatus
    status(const semantic::OperationId& id,
           const semantic::CanonicalCommitStamp& stamp) const noexcept override {
        if (!readStatus || active)
            return {};
        active = true;
        const auto value = readStatus(context, id, stamp);
        active = false;
        if (value.localDurable > 3 || value.cloudSynced > 3)
            return {};
        return {static_cast<interaction::AxisProgress>(value.localDurable),
                static_cast<interaction::AxisProgress>(value.cloudSynced)};
    }
  };
  explicit Impl(std::uint64_t id) : core(render::ViewId{id}), viewId(id) {}
  ~Impl() { (void)core.setLocalOperationSink(nullptr); }
  RuntimeCompositionCore core;
  const std::uint64_t viewId;
  Sink sink;
  const std::thread::id ownerThread = std::this_thread::get_id();
  bool initialResourcesBound = false;
  bool interactionStarted = false;
  bool facadeBorrowed = false;
};
RuntimeCompositionCore& RuntimePlatformAccess::core(RuntimeComposition& owner) noexcept {
  return owner.impl_->core;
}

RuntimeComposition::RuntimeComposition(std::uint64_t viewId) : impl_(std::make_unique<Impl>(viewId)) {}
RuntimeComposition::~RuntimeComposition() = default;
bool RuntimeComposition::open() noexcept { return impl_->core.open(); }
bool RuntimeComposition::openDocument(foundation::ObjectId id) noexcept {
  return impl_->core.openDocument(semantic::DocumentId{id});
}
std::optional<foundation::ObjectId> RuntimeComposition::documentId() const noexcept {
  if (std::this_thread::get_id() != impl_->ownerThread ||
      impl_->core.readback().state == RuntimeCompositionState::kCreated ||
      impl_->core.readback().state == RuntimeCompositionState::kClosed) return std::nullopt;
  return impl_->core.documentId().value();
}
bool RuntimeComposition::setLocalOperationCallback(LocalOperationCallback callback, void* context) noexcept {
    return setLocalOperationCallbacks(callback, context, nullptr, nullptr, nullptr);
}
std::optional<semantic::CanonicalCommitStamp> RuntimeComposition::canonicalCommitFence() const noexcept {
  if (std::this_thread::get_id()!=impl_->ownerThread ||
      impl_->core.readback().state!=RuntimeCompositionState::kReady) return std::nullopt;
  return impl_->core.canonicalCommitFence();
}
bool RuntimeComposition::setLocalOperationCallbacks(LocalOperationCallback callback,
                                                    void* context,
                                                    LocalOperationPrepare prepare,
                                                    LocalOperationAbort abort,
                                                    LocalOperationStatus status) noexcept {
    if (std::this_thread::get_id() != impl_->ownerThread || impl_->sink.active ||
        impl_->core.readback().state != RuntimeCompositionState::kReady)
        return false;
    if (!impl_->core.setLocalOperationSink(callback ? &impl_->sink : nullptr))
        return false;
    impl_->sink.callback = callback;
    impl_->sink.context = callback ? context : nullptr;
    impl_->sink.prepare = callback ? prepare : nullptr;
    impl_->sink.abort = callback ? abort : nullptr;
    impl_->sink.readStatus = callback ? status : nullptr;
    return true;
}
semantic::ApplyResult RuntimeComposition::applyRemoteOperation(const semantic::Operation& operation) noexcept {
  if (std::this_thread::get_id() != impl_->ownerThread || impl_->sink.active) return {};
  return impl_->core.applyExternalOperation(operation, semantic::ApplySource::kRemoteSync);
}
semantic::ApplyResult RuntimeComposition::applyReplayOperation(const semantic::Operation& operation) noexcept {
  if (std::this_thread::get_id()!=impl_->ownerThread || impl_->sink.active) return {};
  return impl_->core.applyExternalOperation(operation,semantic::ApplySource::kRestoreReplay);
}
DocumentSnapshotResult RuntimeComposition::captureDocumentSnapshot(const DocumentSnapshotRequest& request) noexcept {
  if (std::this_thread::get_id() != impl_->ownerThread) {
    DocumentSnapshotResult result{}; result.status = DocumentSnapshotStatus::kWrongThread; return result;
  }
  if (impl_->sink.active) { DocumentSnapshotResult result{}; result.status = DocumentSnapshotStatus::kBusy; return result; }
  return impl_->core.captureDocumentSnapshot(request);
}
DocumentRestoreStatus RuntimeComposition::openDocumentFromSnapshot(const DocumentRestoreRequest& request) noexcept {
  if (std::this_thread::get_id() != impl_->ownerThread) return DocumentRestoreStatus::kWrongThread;
  if (impl_->sink.active || impl_->facadeBorrowed || impl_->core.readback().state != RuntimeCompositionState::kCreated)
    return DocumentRestoreStatus::kNotReady;
  if (request.expectedDocumentId.isZero() || request.resourceBinding == 0) return DocumentRestoreStatus::kInvalidSnapshot;
  try {
    constexpr auto capability = semantic::SnapshotCapability::kAllWithBackgroundV1;
    auto decoded = semantic::SnapshotCodec::decode(request.bytes, capability);
    if (!decoded.ok() || decoded.snapshot->schema_version != 3 || !decoded.snapshot->manifest_present ||
        decoded.snapshot->document_id.value() != request.expectedDocumentId ||
        decoded.snapshot->document_revision != request.documentRevision ||
        decoded.snapshot->recovery_frontier != request.recoveryFrontier ||
        decoded.snapshot->checkpoint_digest != request.checkpointDigest) return DocumentRestoreStatus::kInvalidSnapshot;
    semantic::SnapshotBootstrapper::CheckpointBinding binding{decoded.snapshot->document_id,
        request.documentRevision, request.recoveryFrontier, request.checkpointDigest};
    auto candidate = std::make_unique<Impl>(impl_->viewId);
    if (!candidate->core.openDocument(decoded.snapshot->document_id)) return DocumentRestoreStatus::kFailed;
    const auto outcome = candidate->core.restoreSnapshotBeforeSurface(*decoded.snapshot, binding, request.resourceBinding);
    if (outcome != DocumentRestoreStatus::kRestored) return outcome;
    candidate->initialResourcesBound = true;
    impl_ = std::move(candidate);
    return DocumentRestoreStatus::kRestored;
  } catch (...) { return DocumentRestoreStatus::kFailed; }
}
bool RuntimeComposition::bindSurface(std::uint32_t w, std::uint32_t h) noexcept {
  return impl_->core.bindSurface(w, h);
}
bool RuntimeComposition::registerSource(input::InputSourceId source) noexcept {
  return impl_->core.registerSource(source);
}
bool RuntimeComposition::unregisterSource(input::InputSourceId source, std::uint64_t sequence,
                                           std::uint64_t timestampNs) noexcept {
  return impl_->core.unregisterSource(source, sequence, timestampNs);
}
std::optional<input::PointerKey> RuntimeComposition::beginContact(
    const input::PlatformPointerBatch& batch) noexcept {
  if (batch.samples.size() != 1 || batch.sourceLost || batch.terminalCancel ||
      batch.samples.front().phase != input::PointerPhase::kDown) return std::nullopt;
  const auto& sample = batch.samples.front();
  if (impl_->core.platformKey(sample.source, sample.pointer)) return std::nullopt;
  if (!impl_->core.submit(batch)) {
    // Ingress may have admitted Down before the tool rejected it. Retire the
    // exact generation so a failed begin cannot strand a live contact.
    if (const auto key = impl_->core.platformKey(sample.source, sample.pointer))
      (void)impl_->core.cancelPlatformPointer(*key, sample.sequence, sample.timestampNs);
    return std::nullopt;
  }
  const auto key = impl_->core.platformKey(sample.source, sample.pointer);
  if (key) impl_->interactionStarted = true;
  return key;
}
bool RuntimeComposition::submitContact(const input::PointerKey& key,
    const input::PlatformPointerBatch& batch) noexcept {
  if (!key.valid() || batch.samples.empty() || batch.sourceLost || batch.terminalCancel ||
      impl_->core.platformKey(key.source, key.pointer) != key) return false;
  for (std::size_t i = 0; i < batch.samples.size(); ++i) {
    const auto& sample = batch.samples[i];
    if (sample.source != key.source || sample.pointer != key.pointer ||
        sample.phase == input::PointerPhase::kDown ||
        ((sample.phase == input::PointerPhase::kUp || sample.phase == input::PointerPhase::kCancel) &&
         i + 1 != batch.samples.size())) return false;
  }
  return impl_->core.submit(batch);
}
bool RuntimeComposition::cancelContact(const input::PointerKey& key, std::uint64_t sequence,
                                       std::uint64_t timestampNs) noexcept {
  return impl_->core.cancelPlatformPointer(key, sequence, timestampNs);
}
bool RuntimeComposition::ownerPump() noexcept { return impl_->core.ownerPump(); }
bool RuntimeComposition::bindInitialResourceManifest(const semantic::ResourceManifest& manifest,
                                                      std::uint64_t binding) noexcept {
  if (std::this_thread::get_id() != impl_->ownerThread || impl_->initialResourcesBound || impl_->interactionStarted ||
      impl_->core.readback().state != RuntimeCompositionState::kReady ||
      impl_->core.canonicalCommitFence().ordinal.value() != 0 ||
      impl_->core.controlFacade().readTextEditor().phase != TextPhase::kClosed ||
      impl_->core.semanticObjectCount() != 0 || impl_->core.resources().manifest.manifest_revision != 0 ||
      !impl_->core.resources().manifest.entries.empty() || !impl_->core.resources().manifest.retired_entries.empty()) return false;
  if (!impl_->core.bindManifest(manifest, binding)) return false;
  impl_->initialResourcesBound = true;
  return true;
}
std::optional<ResourceBindingToken> RuntimeComposition::resourceToken(foundation::ObjectId id) const noexcept {
  if (std::this_thread::get_id() != impl_->ownerThread ||
      impl_->core.readback().state != RuntimeCompositionState::kReady) return std::nullopt;
  return impl_->core.currentResourceToken(id);
}
bool RuntimeComposition::publishResourceBytes(const ResourceBindingToken& token,
                                               std::vector<std::uint8_t> bytes) noexcept {
  return impl_->core.publishResource(token, std::move(bytes));
}
std::optional<ResourcePortReceipt> RuntimeComposition::resourceReceipt(foundation::ObjectId id) const noexcept {
  if (std::this_thread::get_id() != impl_->ownerThread ||
      impl_->core.readback().state != RuntimeCompositionState::kReady) return std::nullopt;
  return impl_->core.resourceReceipt(id);
}
std::optional<RuntimeSurfaceMetrics> RuntimeComposition::surfaceMetrics() const noexcept {
  if (std::this_thread::get_id() != impl_->ownerThread) return std::nullopt;
  const auto s = impl_->core.surfaceSnapshot();
  return RuntimeSurfaceMetrics{s.viewId.value(), s.surfaceGeneration, s.metricsGeneration,
      s.logicalWidth, s.logicalHeight, s.width, s.height, s.devicePixelRatio,
      s.displayScale, s.available};
}
std::optional<ResourceBindingSnapshot> RuntimeComposition::resourceSnapshot() const noexcept {
    if (std::this_thread::get_id() != impl_->ownerThread ||
        impl_->core.readback().state != RuntimeCompositionState::kReady)
        return std::nullopt;
    try {
        return impl_->core.resources();
    } catch (...) {
        return std::nullopt;
    }
}
std::optional<std::vector<std::uint8_t>>
RuntimeComposition::readResourceBytes(const ResourceBindingToken& token) const noexcept {
    if (std::this_thread::get_id() != impl_->ownerThread ||
        impl_->core.readback().state != RuntimeCompositionState::kReady)
        return std::nullopt;
    return impl_->core.readResourceBytes(token);
}
std::optional<RuntimeHostObservation> RuntimeComposition::hostObservation() const noexcept {
  if (std::this_thread::get_id() != impl_->ownerThread) return std::nullopt;
  const auto receipt = impl_->core.readback();
  const auto frame = impl_->core.lastSubmittedCanonicalFrame();
  return RuntimeHostObservation{static_cast<std::uint32_t>(receipt.state),
      static_cast<std::uint64_t>(impl_->core.semanticObjectCount()),
      frame ? frame->frameId.value() : 0U,
      static_cast<std::uint64_t>(receipt.lastOwnerControlCount), impl_->core.previewActive()};
}
bool RuntimeComposition::hoverSelectionAtViewPoint(float x, float y) noexcept {
  return impl_->core.hoverSelectionAtViewPoint(x, y);
}
bool RuntimeComposition::close() noexcept {
  if (std::this_thread::get_id() != impl_->ownerThread || impl_->sink.active) return false;
  const bool closed = impl_->core.close();
  if (closed) { (void)impl_->core.setLocalOperationSink(nullptr); impl_->sink.callback = nullptr; }
  return closed;
}
RuntimeFacade& RuntimeComposition::controlFacade() noexcept { impl_->facadeBorrowed = true; return impl_->core.controlFacade(); }
const RuntimeFacade& RuntimeComposition::controlFacade() const noexcept { impl_->facadeBorrowed = true; return impl_->core.controlFacade(); }

}  // namespace canvas::runtime
