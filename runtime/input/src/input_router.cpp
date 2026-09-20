#include "canvas/input/input_router.hpp"

namespace canvas::input {

DispatchDisposition InputRouter::dispatch(const PointerSampleBatch& batch) {
  if (batch.terminalCancel) return DispatchDisposition::kCancelled;
  auto next = lastByPointer_;
  std::uint64_t legacyPrevious = lastConfirmed_;
  for (const auto& sample : batch.samples) {
    if (sample.sequence == 0 || sample.timestampNs == 0) return DispatchDisposition::kRejected;
    const PointerKey key = sample.key.valid() ? sample.key : PointerKey{1, 1, 1};
    const auto previousIt = next.find(key);
    const std::uint64_t previous = previousIt == next.end() ? 0 : previousIt->second;
    if (!sample.predicted) {
      if (sample.sequence <= previous) return DispatchDisposition::kRejected;
      next[key] = sample.sequence;
      legacyPrevious = sample.sequence;
    }
  }
  lastByPointer_ = std::move(next);
  if (!batch.samples.empty()) lastConfirmed_ = legacyPrevious;
  for (const auto& sample : batch.samples) {
    if (sample.predicted) ++predicted_;
    else ++confirmed_;
  }
  return DispatchDisposition::kDelivered;
}

std::uint64_t InputRouter::lastConfirmedSequence(const PointerKey& key) const noexcept {
  const auto it = lastByPointer_.find(key);
  return it == lastByPointer_.end() ? 0 : it->second;
}

}  // namespace canvas::input
