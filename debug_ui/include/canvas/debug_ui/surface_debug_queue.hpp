#pragma once

#include "canvas/runtime/surface_debug_control.hpp"

#include <cstddef>
#include <deque>
#include <mutex>
#include <optional>
#include <unordered_map>

namespace canvas::runtime {

inline constexpr std::size_t kDefaultSurfaceModeQueueCapacity = 256;
inline constexpr std::size_t kDefaultSurfaceModeReceiptCapacity = 1024;

// Reference bounded queue for Platform SurfaceRebind admission. It owns only
// typed requests and receipts; the platform owner still performs the actual
// lifecycle work at its declared safe point.
class BoundedSurfaceModeQueue final {
 public:
  explicit BoundedSurfaceModeQueue(
      std::size_t capacity = kDefaultSurfaceModeQueueCapacity,
      std::size_t receiptCapacity = kDefaultSurfaceModeReceiptCapacity)
      : capacity_(capacity), receiptCapacity_(receiptCapacity) {}

  [[nodiscard]] SurfaceModeReceipt enqueue(const SurfaceModeRequest& request,
                                           std::uint64_t currentGeneration,
                                           std::uint64_t currentSequence = 0) {
    std::lock_guard lock(mutex_);
    SurfaceModeReceipt receipt{request.requestId, SurfaceControlState::kQueued,
                               request.target, request.mode, currentGeneration};
    if (request.expectedGeneration != currentGeneration) {
      receipt.state = SurfaceControlState::kStaleGeneration;
      return receipt;
    }
    if (request.deadlineSequence != 0 && currentSequence > request.deadlineSequence) {
      receipt.state = SurfaceControlState::kExpired;
      return receipt;
    }
    if (capacity_ == 0 || receiptCapacity_ == 0 || pending_.size() >= capacity_) {
      receipt.state = SurfaceControlState::kQueueFull;
      return receipt;
    }
    if (receipts_.contains(request.requestId)) {
      receipt.state = SurfaceControlState::kQueueFull;
      return receipt;
    }
    if (receipts_.size() >= receiptCapacity_ && !evictOneTerminal()) {
      receipt.state = SurfaceControlState::kQueueFull;
      return receipt;
    }
    pending_.push_back(request);
    receipts_[request.requestId] = receipt;
    receiptOrder_.push_back(request.requestId);
    return receipt;
  }

  [[nodiscard]] std::optional<SurfaceModeRequest> take(
      std::uint64_t currentGeneration, std::uint64_t currentSequence = 0) {
    std::lock_guard lock(mutex_);
    if (pending_.empty()) return std::nullopt;
    if (pending_.front().expectedGeneration != currentGeneration) {
      auto it = receipts_.find(pending_.front().requestId);
      if (it != receipts_.end()) it->second.state = SurfaceControlState::kStaleGeneration;
      pending_.pop_front();
      return std::nullopt;
    }
    if (pending_.front().deadlineSequence != 0 &&
        currentSequence > pending_.front().deadlineSequence) {
      auto it = receipts_.find(pending_.front().requestId);
      if (it != receipts_.end()) it->second.state = SurfaceControlState::kExpired;
      pending_.pop_front();
      return std::nullopt;
    }
    auto request = pending_.front();
    pending_.pop_front();
    return request;
  }

  [[nodiscard]] std::size_t expire(std::uint64_t currentSequence) {
    std::lock_guard lock(mutex_);
    std::size_t count = 0;
    for (auto it = pending_.begin(); it != pending_.end();) {
      if (it->deadlineSequence != 0 && currentSequence > it->deadlineSequence) {
        auto receipt = receipts_.find(it->requestId);
        if (receipt != receipts_.end()) receipt->second.state = SurfaceControlState::kExpired;
        it = pending_.erase(it);
        ++count;
      } else {
        ++it;
      }
    }
    return count;
  }

  [[nodiscard]] SurfaceModeReceipt complete(std::uint64_t requestId,
                                             SurfaceControlState state,
                                             std::uint64_t generation) {
    std::lock_guard lock(mutex_);
    auto it = receipts_.find(requestId);
    if (it == receipts_.end()) {
      return SurfaceModeReceipt{requestId, state, SurfaceRole::kCanonicalCanvas,
                                SurfaceMode::kPlatformDefault, generation};
    }
    it->second.state = state;
    it->second.generation = generation;
    return it->second;
  }

  [[nodiscard]] std::optional<SurfaceModeReceipt> receipt(std::uint64_t requestId) const {
    std::lock_guard lock(mutex_);
    const auto it = receipts_.find(requestId);
    return it == receipts_.end() ? std::nullopt : std::optional<SurfaceModeReceipt>(it->second);
  }

  // Returns the newest retained receipt without exposing the queue internals.
  // Platform diagnostics use this to publish the owner result in the next
  // immutable snapshot; the UI never treats admission as application.
  [[nodiscard]] std::optional<SurfaceModeReceipt> latestReceipt() const {
    std::lock_guard lock(mutex_);
    for (auto it = receiptOrder_.rbegin(); it != receiptOrder_.rend(); ++it) {
      const auto receipt = receipts_.find(*it);
      if (receipt != receipts_.end()) return receipt->second;
    }
    return std::nullopt;
  }

  [[nodiscard]] std::size_t destroyGeneration(std::uint64_t generation) {
    std::lock_guard lock(mutex_);
    std::size_t count = 0;
    for (auto it = pending_.begin(); it != pending_.end();) {
      if (it->expectedGeneration == generation) {
        auto receipt = receipts_.find(it->requestId);
        if (receipt != receipts_.end()) receipt->second.state = SurfaceControlState::kUnavailable;
        it = pending_.erase(it);
        ++count;
      } else {
        ++it;
      }
    }
    return count;
  }

  [[nodiscard]] std::size_t size() const {
    std::lock_guard lock(mutex_);
    return pending_.size();
  }

 private:
  [[nodiscard]] bool evictOneTerminal() {
    for (auto it = receiptOrder_.begin(); it != receiptOrder_.end(); ++it) {
      const auto receipt = receipts_.find(*it);
      if (receipt == receipts_.end()) continue;
      if (receipt->second.state == SurfaceControlState::kQueued) continue;
      receipts_.erase(receipt);
      receiptOrder_.erase(it);
      return true;
    }
    return false;
  }

  const std::size_t capacity_;
  const std::size_t receiptCapacity_;
  mutable std::mutex mutex_;
  std::deque<SurfaceModeRequest> pending_;
  std::unordered_map<std::uint64_t, SurfaceModeReceipt> receipts_;
  std::deque<std::uint64_t> receiptOrder_;
};

}  // namespace canvas::runtime
