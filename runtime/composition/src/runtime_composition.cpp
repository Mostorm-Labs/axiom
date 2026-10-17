#include "canvas/runtime/runtime_composition.hpp"

namespace canvas::runtime {

RuntimeComposition::RuntimeComposition()
    : runtime_(std::make_unique<interaction::CanvasEditingRuntime>()),
      provider_(std::make_unique<MemoryResourceProvider>()) {}
RuntimeComposition::~RuntimeComposition() { (void)close(); }
bool RuntimeComposition::transition(RuntimeCompositionState next) noexcept {
  if (state_ == RuntimeCompositionState::kClosed) return false;
  state_ = next; return true;
}
bool RuntimeComposition::canMutate() const noexcept { return state_ == RuntimeCompositionState::kReady; }
bool RuntimeComposition::open() noexcept {
  return state_ == RuntimeCompositionState::kCreated && transition(RuntimeCompositionState::kBound);
}
bool RuntimeComposition::bindSurface(std::uint32_t width, std::uint32_t height) noexcept {
  return state_ == RuntimeCompositionState::kBound && runtime_->bindSurface(width, height) && transition(RuntimeCompositionState::kReady);
}
bool RuntimeComposition::bindManifest(const semantic::ResourceManifest& manifest, std::uint64_t binding) noexcept {
  return (state_ == RuntimeCompositionState::kBound || canMutate()) && resources_.bind(manifest, binding);
}
bool RuntimeComposition::registerSource(input::InputSourceId source) noexcept {
  if (state_ == RuntimeCompositionState::kBound && runtime_->surface().available) {
    state_ = RuntimeCompositionState::kReady;
  }
  return (state_ == RuntimeCompositionState::kBound || canMutate()) &&
      runtime_->registerInputSource(source, "composition");
}
bool RuntimeComposition::submit(const input::PlatformPointerBatch& batch) noexcept {
  if (state_ == RuntimeCompositionState::kBound && runtime_->surface().available) {
    state_ = RuntimeCompositionState::kReady;
  }
  return canMutate() && !batch.samples.empty() &&
      runtime_->acceptPlatformBatch(batch, batch.samples.back().timestampNs);
}
bool RuntimeComposition::notifyResource(foundation::ObjectId id) noexcept { if (!canMutate() || notifications_.size() >= 256U) return false; notifications_.push_back({id, nextRequestId_++}); return true; }
bool RuntimeComposition::publishResource(foundation::ObjectId id, std::vector<std::uint8_t> bytes) noexcept {
  if (!canMutate() || resources_.find(id) == nullptr) return false;
  provider_->publish(id, std::move(bytes));
  ++providerEpoch_;
  return notifyResource(id);
}
bool RuntimeComposition::ownerPump() noexcept {
  if (!canMutate()) return false;
  ++ownerPumpCount_;
  for (const auto& notification : notifications_) {
    const auto* entry = resources_.find(notification.resourceId);
    const auto pulled = provider_->resolve(notification.resourceId);
    ResourcePortReceipt receipt{};
    receipt.resourceId = notification.resourceId;
    receipt.providerGeneration = pulled.generation;
    receipt.byteLength = pulled.bytes ? pulled.bytes->size() : 0U;
    receipt.token = resources_.token(notification.resourceId, providerEpoch_, notification.sequence).value_or(ResourceBindingToken{});
    receipt.state = entry == nullptr ? ResourceBindingState::kMissing
        : !pulled.bytes ? ResourceBindingState::kMissing
        : validateResourceBytes(*entry, pulled) ? ResourceBindingState::kBytesValidated
                                                : ResourceBindingState::kFailed;
    const auto old = resourceReceipts_.find(notification.resourceId);
    receipt.changed = old == resourceReceipts_.end() || old->second.providerGeneration != receipt.providerGeneration || old->second.state != receipt.state;
    resourceReceipts_[notification.resourceId] = std::move(receipt);
  }
  notifications_.clear();
  return true;
}
bool RuntimeComposition::requestCanonicalFrame(std::uint64_t frameId) noexcept { return canMutate() && runtime_->presentCanonicalFrame(frameId, 0.0, false); }
bool RuntimeComposition::receivePlatformPresented(const render::PresentedFeedback& feedback) noexcept { return canMutate() && runtime_->receivePlatformPresented(feedback); }
bool RuntimeComposition::stop() noexcept { if (state_ != RuntimeCompositionState::kReady && state_ != RuntimeCompositionState::kLost) return false; resources_.invalidate(); notifications_.clear(); runtime_->cancelAllPointers(); return transition(RuntimeCompositionState::kStopping) && transition(RuntimeCompositionState::kQuiescent); }
bool RuntimeComposition::close() noexcept { if (state_ == RuntimeCompositionState::kClosed) return true; if (state_ == RuntimeCompositionState::kReady || state_ == RuntimeCompositionState::kLost) (void)stop(); resources_.invalidate(); notifications_.clear(); state_ = RuntimeCompositionState::kClosed; return true; }
RuntimeCompositionReceipt RuntimeComposition::readback() const noexcept {
  const auto& runtime = *runtime_;
  return {state_, resources_.lifetimeGeneration, ownerPumpCount_, resources_.manifest.manifest_revision, runtime.sceneRevision(), runtime.semanticGeneration().value(), notifications_.size(), runtime.lastSubmittedCanonicalFrame().has_value()};
}
std::optional<ResourcePortReceipt> RuntimeComposition::resourceReceipt(
    foundation::ObjectId id) const noexcept {
  const auto it = resourceReceipts_.find(id);
  return it == resourceReceipts_.end() ? std::nullopt : std::optional<ResourcePortReceipt>(it->second);
}
}  // namespace canvas::runtime
