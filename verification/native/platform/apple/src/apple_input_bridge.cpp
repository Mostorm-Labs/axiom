#include <axiom/verification/apple_harness_adapter.hpp>

#include <utility>

namespace axiom::verification::platform {

ApplePointerSampleBatch normalize_apple_pointer_history(
    std::string correlation_id,
    const std::vector<AppleMotionSample>& historical_samples,
    AppleMotionSample current_sample) {
  ApplePointerSampleBatch result{std::move(correlation_id), historical_samples};
  result.samples.push_back(current_sample);
  return result;
}

}  // namespace axiom::verification::platform
