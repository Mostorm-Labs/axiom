#include "canvas/runtime/runtime.hpp"

#include "canvas/runtime/internal/runtime_composition_core.hpp"

namespace canvas::runtime {

class Runtime::Impl final {
 public:
  RuntimeCompositionCore composition;
};

Runtime::Runtime() : impl_(std::make_unique<Impl>()) {}
Runtime::~Runtime() = default;
bool Runtime::open() noexcept { return impl_->composition.open(); }
bool Runtime::bindSurface(std::uint32_t width, std::uint32_t height) noexcept {
  return impl_->composition.bindSurface(width, height);
}
bool Runtime::registerSource(input::InputSourceId source) noexcept { return impl_->composition.registerSource(source); }
bool Runtime::unregisterSource(input::InputSourceId source, std::uint64_t sequence, std::uint64_t timestampNs) noexcept {
  return impl_->composition.unregisterSource(source, sequence, timestampNs);
}
bool Runtime::submit(const input::PlatformPointerBatch& batch) noexcept { return impl_->composition.submit(batch); }
bool Runtime::pump() noexcept { return impl_->composition.ownerPump(); }
bool Runtime::close() noexcept { return impl_->composition.close(); }
RuntimeFacade& Runtime::facade() noexcept { return impl_->composition.controlFacade(); }
const RuntimeFacade& Runtime::facade() const noexcept { return impl_->composition.controlFacade(); }

}  // namespace canvas::runtime
