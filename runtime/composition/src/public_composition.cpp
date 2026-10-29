#include "canvas/runtime/runtime_composition.hpp"

#include "canvas/runtime/internal/runtime_composition_core.hpp"
#include "canvas/runtime/internal/runtime_platform_access.hpp"

namespace canvas::runtime {

class RuntimeComposition::Impl final {
 public:
  explicit Impl(std::uint64_t viewId) : core(render::ViewId{viewId}) {}
  RuntimeCompositionCore core;
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
bool RuntimeComposition::submit(const input::PlatformPointerBatch& batch) noexcept {
  return impl_->core.submit(batch);
}
bool RuntimeComposition::ownerPump() noexcept { return impl_->core.ownerPump(); }
bool RuntimeComposition::close() noexcept { return impl_->core.close(); }
RuntimeFacade& RuntimeComposition::controlFacade() noexcept { return impl_->core.controlFacade(); }
const RuntimeFacade& RuntimeComposition::controlFacade() const noexcept { return impl_->core.controlFacade(); }

}  // namespace canvas::runtime
