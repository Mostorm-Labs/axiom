#include "canvas/runtime/runtime_composition.hpp"

#include "canvas/runtime/internal/runtime_composition_core.hpp"
#include "canvas/runtime/internal/runtime_platform_access.hpp"
#include <thread>

namespace canvas::runtime {

class RuntimeComposition::Impl final {
 public:
  explicit Impl(std::uint64_t viewId) : core(render::ViewId{viewId}) {}
  RuntimeCompositionCore core;
  const std::thread::id ownerThread = std::this_thread::get_id();
};
RuntimeCompositionCore& RuntimePlatformAccess::core(RuntimeComposition& owner) noexcept {
  return owner.impl_->core;
}

RuntimeComposition::RuntimeComposition(std::uint64_t viewId) : impl_(std::make_unique<Impl>(viewId)) {}
RuntimeComposition::~RuntimeComposition() = default;
bool RuntimeComposition::open() noexcept { return impl_->core.open(); }
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
  return impl_->core.platformKey(sample.source, sample.pointer);
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
std::optional<RuntimeSurfaceMetrics> RuntimeComposition::surfaceMetrics() const noexcept {
  if (std::this_thread::get_id() != impl_->ownerThread) return std::nullopt;
  const auto s = impl_->core.surfaceSnapshot();
  return RuntimeSurfaceMetrics{s.viewId.value(), s.surfaceGeneration, s.metricsGeneration,
      s.logicalWidth, s.logicalHeight, s.width, s.height, s.devicePixelRatio,
      s.displayScale, s.available};
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
bool RuntimeComposition::close() noexcept { return impl_->core.close(); }
RuntimeFacade& RuntimeComposition::controlFacade() noexcept { return impl_->core.controlFacade(); }
const RuntimeFacade& RuntimeComposition::controlFacade() const noexcept { return impl_->core.controlFacade(); }

}  // namespace canvas::runtime
