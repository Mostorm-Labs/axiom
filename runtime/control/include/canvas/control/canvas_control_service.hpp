#pragma once

#include "canvas/control/canvas_control_port.hpp"

#include <cstddef>
#include <deque>
#include <mutex>
#include <optional>
#include <unordered_map>
#include <unordered_set>

namespace canvas::runtime {

class CanvasControlService final {
 public:
  explicit CanvasControlService(CanvasControlPort& port, std::size_t queueCapacity = 256,
                                std::size_t receiptCapacity = 1024);
  [[nodiscard]] CanvasControlReceipt enqueue(const CanvasControlRequest& request);
  [[nodiscard]] std::size_t processPending(std::uint64_t ownerSequence);
  [[nodiscard]] std::optional<CanvasControlReceipt> receipt(CanvasControlReceiptKey key) const;
  [[nodiscard]] CanvasControlSnapshot snapshot(const CanvasTargetKey& target) const;
  void destroyTarget(const CanvasTargetKey& target);
  [[nodiscard]] std::size_t pending() const noexcept;

 private:
  void remember(CanvasControlReceipt receipt);
  [[nodiscard]] bool wasDestroyed(const CanvasTargetKey& target) const;

  CanvasControlPort& port_;
  const std::size_t queueCapacity_;
  const std::size_t receiptCapacity_;
  mutable std::mutex mutex_;
  std::deque<CanvasControlRequest> queue_;
  std::deque<CanvasControlReceiptKey> receiptOrder_;
  std::unordered_map<CanvasControlReceiptKey, CanvasControlReceipt,
                     CanvasControlReceiptKeyHash> receipts_;
  // Clients allocate strictly increasing request IDs. Never evict their
  // high-water marks: old IDs remain non-executable after receipt eviction.
  // The client registry itself is bounded; new clients are refused when full.
  std::unordered_map<std::uint64_t, std::uint64_t> clientHighWater_;
  std::unordered_set<CanvasTargetKey, CanvasTargetKeyHash> destroyed_;
};

}  // namespace canvas::runtime
