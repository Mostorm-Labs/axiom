#pragma once
#include "canvas/input/pointer_sample_batch.hpp"
#include <cstddef>
#include <cstdint>
#include <unordered_map>

namespace canvas::input {
enum class DispatchDisposition : std::uint8_t { kDelivered, kCancelled, kRejected };

class InputRouter final {
 public:
  DispatchDisposition dispatch(const PointerSampleBatch&);
  std::size_t confirmedCount() const noexcept { return confirmed_; }
  std::size_t predictedCount() const noexcept { return predicted_; }
  std::uint64_t lastConfirmedSequence() const noexcept { return lastConfirmed_; }
  [[nodiscard]] std::uint64_t lastConfirmedSequence(const PointerKey& key) const noexcept;

 private:
  std::size_t confirmed_ = 0;
  std::size_t predicted_ = 0;
  std::uint64_t lastConfirmed_ = 0;
  std::unordered_map<PointerKey, std::uint64_t, PointerKeyHash> lastByPointer_;
};
}  // namespace canvas::input
