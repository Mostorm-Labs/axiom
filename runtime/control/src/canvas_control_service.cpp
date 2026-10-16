#include "canvas/control/canvas_control_service.hpp"

#include <algorithm>

namespace canvas::runtime {

namespace {
CanvasControlReceipt makeReceipt(CanvasControlReceiptKey key,
                                 CanvasControlReceiptState state,
                                 CanvasControlError error = CanvasControlError::kNone,
                                 std::uint64_t appliedRevision = 0,
                                 std::string detail = {}) {
  CanvasControlReceipt receipt{};
  receipt.key = key;
  receipt.state = state;
  receipt.error = error;
  receipt.appliedRevision = appliedRevision;
  receipt.detail = std::move(detail);
  return receipt;
}
}  // namespace

CanvasControlService::CanvasControlService(CanvasControlPort& port, std::size_t queueCapacity,
                                           std::size_t receiptCapacity)
    : port_(port), queueCapacity_(std::max<std::size_t>(1, queueCapacity)),
      receiptCapacity_(std::max<std::size_t>(1, receiptCapacity)) {}

void CanvasControlService::remember(CanvasControlReceipt receipt) {
  const auto key = receipt.key;
  if (receipts_.find(key) != receipts_.end()) {
    receipts_[key] = std::move(receipt);
    return;
  }
  receipts_[key] = std::move(receipt);
  receiptOrder_.push_back(key);
  while (receiptOrder_.size() > receiptCapacity_) {
    const auto old = receiptOrder_.front();
    receiptOrder_.pop_front();
    receipts_.erase(old);
  }
}

bool CanvasControlService::wasDestroyed(const CanvasTargetKey& target) const {
  return destroyed_.find(target) != destroyed_.end();
}

bool CanvasControlService::isSelectionScoped(CanvasControlPayloadKind kind) noexcept {
  switch (kind) {
    case CanvasControlPayloadKind::kSelectedAppearance:
    case CanvasControlPayloadKind::kSelectedSize:
    case CanvasControlPayloadKind::kSelectedTransform:
    case CanvasControlPayloadKind::kSelectedOrder:
    case CanvasControlPayloadKind::kSelectedLock:
    case CanvasControlPayloadKind::kSelectedVisible:
    case CanvasControlPayloadKind::kDeleteSelected:
    case CanvasControlPayloadKind::kUnlockSelected:
      return true;
    default:
      return false;
  }
}

CanvasControlReceipt CanvasControlService::enqueue(const CanvasControlRequest& request) {
  std::scoped_lock lock(mutex_);
  const CanvasControlReceiptKey key{request.clientId, request.requestId};
  const auto client = clientHighWater_.find(request.clientId);
  if (client != clientHighWater_.end() && request.requestId <= client->second &&
      receipts_.find(key) == receipts_.end()) {
    return makeReceipt(key, CanvasControlReceiptState::kExpired);
  }
  if (client != clientHighWater_.end() && request.requestId <= client->second) {
    return makeReceipt(key, CanvasControlReceiptState::kDuplicate);
  }
  if (client == clientHighWater_.end() &&
      clientHighWater_.size() >= queueCapacity_ + receiptCapacity_) {
    return makeReceipt(key, CanvasControlReceiptState::kQueueFull,
                       CanvasControlError::kQueueFull, 0U, "client registry full");
  }
  const auto capture = port_.snapshot(request.target);
  if (wasDestroyed(request.target) || !port_.isTargetCurrent(request.target)) {
    CanvasControlReceipt receipt = makeReceipt(key, CanvasControlReceiptState::kStaleTarget,
                                               CanvasControlError::kStaleTarget);
    remember(receipt);
    clientHighWater_[request.clientId] = request.requestId;
    return receipt;
  }
  if (queue_.size() >= queueCapacity_) {
    CanvasControlReceipt receipt = makeReceipt(key, CanvasControlReceiptState::kQueueFull,
                                               CanvasControlError::kQueueFull);
    remember(receipt);
    clientHighWater_[request.clientId] = request.requestId;
    return receipt;
  }
  clientHighWater_[request.clientId] = request.requestId;
  queue_.push_back(request);
  if (isSelectionScoped(request.payload.kind)) capturedSnapshots_[key] = capture;
  CanvasControlReceipt receipt = makeReceipt(key, CanvasControlReceiptState::kQueued);
  remember(receipt);
  return receipt;
}

std::size_t CanvasControlService::processPending(std::uint64_t ownerSequence) {
  std::size_t processed = 0;
  for (;;) {
    CanvasControlRequest request{};
    std::optional<CanvasControlSnapshot> capturedSnapshot;
    {
      std::scoped_lock lock(mutex_);
      if (queue_.empty()) break;
      request = queue_.front();
      queue_.pop_front();
      const CanvasControlReceiptKey key{request.clientId, request.requestId};
      const auto captured = capturedSnapshots_.find(key);
      if (captured != capturedSnapshots_.end()) capturedSnapshot = captured->second;
      if (wasDestroyed(request.target)) {
        remember(makeReceipt(key, CanvasControlReceiptState::kTargetDestroyed,
                             CanvasControlError::kStaleTarget));
        capturedSnapshots_.erase(key);
        continue;
      }
      if (request.deadlineSequence != 0 && ownerSequence > request.deadlineSequence) {
        remember(makeReceipt(key, CanvasControlReceiptState::kExpired));
        capturedSnapshots_.erase(key);
        continue;
      }
      if (!port_.isTargetCurrent(request.target)) {
        remember(makeReceipt(key, CanvasControlReceiptState::kStaleTarget,
                             CanvasControlError::kStaleTarget));
        capturedSnapshots_.erase(key);
        continue;
      }
      if (capturedSnapshot.has_value()) {
        const auto current = port_.snapshot(request.target);
        if (capturedSnapshot->selectedObject != current.selectedObject) {
          capturedSnapshots_.erase(key);
          remember(makeReceipt(key, CanvasControlReceiptState::kStaleTarget,
                               CanvasControlError::kStaleTarget));
          continue;
        }
        if (request.payload.kind == CanvasControlPayloadKind::kSelectedAppearance) {
          auto desired = request.payload.appearance;
          const auto& base = capturedSnapshot->selectedAppearance;
          if (desired.fillEnabled == base.fillEnabled && desired.fill == base.fill) {
            desired.fillEnabled = current.selectedAppearance.fillEnabled;
            desired.fill = current.selectedAppearance.fill;
          }
          if (desired.strokeEnabled == base.strokeEnabled && desired.stroke == base.stroke) {
            desired.strokeEnabled = current.selectedAppearance.strokeEnabled;
            desired.stroke = current.selectedAppearance.stroke;
          }
          if (desired.opacity == base.opacity) desired.opacity = current.selectedAppearance.opacity;
          request.payload.appearance = desired;
        }
      }
    }
    const auto result = port_.apply(request);
    const CanvasControlReceiptKey key{request.clientId, request.requestId};
    const auto state = result.error == CanvasControlError::kNone
        ? CanvasControlReceiptState::kApplied
        : result.error == CanvasControlError::kStaleTarget
            ? CanvasControlReceiptState::kStaleTarget
            : CanvasControlReceiptState::kFailed;
    CanvasControlReceipt receipt = makeReceipt(
        key, state,
        result.error);
    receipt.detail = result.detail == nullptr ? "" : result.detail;
    receipt.appliedRevision = port_.snapshot(request.target).controlRevision;
    {
      std::scoped_lock lock(mutex_);
      capturedSnapshots_.erase(key);
      remember(std::move(receipt));
    }
    ++processed;
  }
  return processed;
}

std::optional<CanvasControlReceipt> CanvasControlService::receipt(CanvasControlReceiptKey key) const {
  std::scoped_lock lock(mutex_);
  const auto it = receipts_.find(key);
  return it == receipts_.end() ? std::nullopt : std::optional<CanvasControlReceipt>(it->second);
}

CanvasControlSnapshot CanvasControlService::snapshot(const CanvasTargetKey& target) const {
  return port_.snapshot(target);
}

void CanvasControlService::destroyTarget(const CanvasTargetKey& target) {
  std::scoped_lock lock(mutex_);
  destroyed_.insert(target);
  for (auto it = queue_.begin(); it != queue_.end();) {
    if (it->target == target) {
      remember(makeReceipt({it->clientId, it->requestId},
                           CanvasControlReceiptState::kTargetDestroyed,
                           CanvasControlError::kStaleTarget));
      it = queue_.erase(it);
    } else {
      ++it;
    }
  }
}

std::size_t CanvasControlService::pending() const noexcept {
  std::scoped_lock lock(mutex_);
  return queue_.size();
}

}  // namespace canvas::runtime
