#include "canvas/runtime/runtime.hpp"

#include "canvas/runtime/runtime_composition.hpp"

namespace canvas::runtime {

class Runtime::Impl final {
 public:
  RuntimeComposition composition;
};

Runtime::Runtime() : impl_(std::make_unique<Impl>()) {}
Runtime::~Runtime() = default;
bool Runtime::open() noexcept { return impl_->composition.open(); }
bool Runtime::bindSurface(std::uint32_t width, std::uint32_t height) noexcept {
  return impl_->composition.bindSurface(width, height);
}
bool Runtime::pump() noexcept { return impl_->composition.ownerPump(); }
bool Runtime::close() noexcept { return impl_->composition.close(); }
RuntimeFacade& Runtime::facade() noexcept { return impl_->composition.controlFacade(); }
const RuntimeFacade& Runtime::facade() const noexcept { return impl_->composition.controlFacade(); }

}  // namespace canvas::runtime
