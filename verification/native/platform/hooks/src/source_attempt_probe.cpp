#include <axiom/verification/platform_hooks.h>

namespace axiom::verification::platform {

void SourceAttemptProbe::record(SourceAttempt attempt) {
  records_.push_back(std::move(attempt));
}

const std::vector<SourceAttempt>& SourceAttemptProbe::records() const noexcept {
  return records_;
}

}  // namespace axiom::verification::platform
