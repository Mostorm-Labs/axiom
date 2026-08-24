#include <axiom/verification/windows_harness_adapter.hpp>

#include <utility>

namespace axiom::verification::platform {

PointerSampleBatch normalize_windows_pointer_history(
    std::string correlation_id, const std::vector<NativePointerHistory>& history) {
  PointerSampleBatch result{std::move(correlation_id), {}};
  result.samples.reserve(history.size());
  for (const auto& sample : history) {
    result.samples.push_back({sample.x, sample.y, sample.pressure, sample.tilt_x,
                              sample.tilt_y, sample.timestamp});
  }
  return result;
}

}  // namespace axiom::verification::platform
