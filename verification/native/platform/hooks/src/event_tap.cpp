#include <axiom/verification/platform_hooks.h>

namespace axiom::verification::platform {

void EventTap::record(TappedEvent event) { records_.push_back(std::move(event)); }

const std::vector<TappedEvent>& EventTap::records() const noexcept {
  return records_;
}

}  // namespace axiom::verification::platform
