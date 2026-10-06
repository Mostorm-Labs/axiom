#pragma once

#include "canvas/runtime/debug_control.hpp"

#include <cstddef>
#include <deque>
#include <mutex>
#include <optional>
#include <unordered_map>

namespace canvas::runtime {

inline constexpr std::size_t kDefaultAxiomDebugCommandQueueCapacity = 256;
inline constexpr std::size_t kDefaultAxiomDebugCommandReceiptCapacity = 1024;

// Reference bounded mutex queue. It stores typed commands only; an owner must
// apply a taken command at its declared safe point and complete its receipt.
class BoundedAxiomDebugCommandQueue final {
 public:
  explicit BoundedAxiomDebugCommandQueue(
      std::size_t capacity = kDefaultAxiomDebugCommandQueueCapacity,
      std::size_t receiptCapacity = kDefaultAxiomDebugCommandReceiptCapacity)
      : capacity_(capacity), receiptCapacity_(receiptCapacity) {}

  [[nodiscard]] AxiomDebugCommandReceipt enqueue(const AxiomDebugCommand& command,
                                                  std::uint64_t runtimeGeneration,
                                                  std::uint64_t documentGeneration,
                                                  std::uint64_t sequence) {
    std::lock_guard lock(mutex_);
    AxiomDebugCommandReceipt receipt{command.requestId,
                                     AxiomDebugCommandState::kQueued, 0,
                                     runtimeGeneration, documentGeneration};
    if (command.requestId == 0U || command.expectedRuntimeGeneration != 0U &&
        command.expectedRuntimeGeneration != runtimeGeneration ||
        command.expectedDocumentGeneration != 0U &&
        command.expectedDocumentGeneration != documentGeneration) {
      receipt.state = AxiomDebugCommandState::kStaleGeneration;
      return receipt;
    }
    if (command.deadlineSequence != 0U && sequence > command.deadlineSequence) {
      receipt.state = AxiomDebugCommandState::kExpired;
      return receipt;
    }
    if (capacity_ == 0U || pending_.size() >= capacity_ ||
        receiptCapacity_ == 0U || receipts_.contains(command.requestId)) {
      receipt.state = AxiomDebugCommandState::kQueueFull;
      return receipt;
    }
    if (receipts_.size() >= receiptCapacity_ && !evictTerminalLocked()) {
      receipt.state = AxiomDebugCommandState::kQueueFull;
      return receipt;
    }
    pending_.push_back(command);
    receipts_[command.requestId] = receipt;
    order_.push_back(command.requestId);
    return receipt;
  }

  [[nodiscard]] std::optional<AxiomDebugCommand> take(
      std::uint64_t runtimeGeneration, std::uint64_t documentGeneration,
      std::uint64_t sequence) {
    std::lock_guard lock(mutex_);
    for (auto it = pending_.begin(); it != pending_.end();) {
      if (it->expectedRuntimeGeneration != 0U &&
          it->expectedRuntimeGeneration != runtimeGeneration ||
          it->expectedDocumentGeneration != 0U &&
          it->expectedDocumentGeneration != documentGeneration) {
        terminalLocked(it->requestId, AxiomDebugCommandState::kStaleGeneration);
        it = pending_.erase(it);
        continue;
      }
      if (it->deadlineSequence != 0U && sequence > it->deadlineSequence) {
        terminalLocked(it->requestId, AxiomDebugCommandState::kExpired);
        it = pending_.erase(it);
        continue;
      }
      auto command = *it;
      pending_.erase(it);
      return command;
    }
    return std::nullopt;
  }

  [[nodiscard]] AxiomDebugCommandReceipt complete(
      std::uint64_t requestId, AxiomDebugCommandState state,
      std::uint64_t frameId, std::uint64_t runtimeGeneration,
      std::uint64_t documentGeneration) {
    std::lock_guard lock(mutex_);
    auto it = receipts_.find(requestId);
    if (it == receipts_.end()) {
      return {requestId, state, frameId, runtimeGeneration, documentGeneration};
    }
    if (it->second.state == AxiomDebugCommandState::kQueued) {
      it->second.state = state;
      it->second.appliedFrameId = frameId;
      it->second.runtimeGeneration = runtimeGeneration;
      it->second.documentGeneration = documentGeneration;
    }
    return it->second;
  }

  [[nodiscard]] std::optional<AxiomDebugCommandReceipt> receipt(
      std::uint64_t requestId) const {
    std::lock_guard lock(mutex_);
    const auto it = receipts_.find(requestId);
    return it == receipts_.end() ? std::nullopt
                                 : std::optional<AxiomDebugCommandReceipt>(it->second);
  }

 private:
  void terminalLocked(std::uint64_t id, AxiomDebugCommandState state) {
    const auto it = receipts_.find(id);
    if (it != receipts_.end() && it->second.state == AxiomDebugCommandState::kQueued) {
      it->second.state = state;
    }
  }
  [[nodiscard]] bool evictTerminalLocked() {
    for (auto it = order_.begin(); it != order_.end(); ++it) {
      const auto receipt = receipts_.find(*it);
      if (receipt == receipts_.end() || receipt->second.state == AxiomDebugCommandState::kQueued) {
        continue;
      }
      receipts_.erase(receipt);
      order_.erase(it);
      return true;
    }
    return false;
  }

  const std::size_t capacity_;
  const std::size_t receiptCapacity_;
  mutable std::mutex mutex_;
  std::deque<AxiomDebugCommand> pending_;
  std::unordered_map<std::uint64_t, AxiomDebugCommandReceipt> receipts_;
  std::deque<std::uint64_t> order_;
};

}  // namespace canvas::runtime
