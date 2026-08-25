#include <axiom/verification/android_harness_adapter.hpp>

#include <utility>

namespace axiom::verification::platform {

AndroidPointerSampleBatch normalize_android_motion_history(
    std::string correlation_id,
    const std::vector<AndroidMotionSample>& historical_samples,
    AndroidMotionSample current_sample) {
  AndroidPointerSampleBatch result{std::move(correlation_id), historical_samples};
  result.samples.push_back(current_sample);
  return result;
}

}  // namespace axiom::verification::platform
