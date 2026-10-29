#include "canvas/control/canvas_control_service.hpp"
#include "multi_selection_queue.hpp"

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
    const auto eligible=std::find_if(receiptOrder_.begin(),receiptOrder_.end(),[&](auto old){
      return receipts_.at(old).state!=CanvasControlReceiptState::kQueued;
    });
    if(eligible==receiptOrder_.end())break; // a queued command retains its reserved terminal slot
    const auto old = *eligible;
    receiptOrder_.erase(eligible);
    receipts_.erase(old);
    sectionRequests_.erase(old);
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
    if(const auto prior=sectionRequests_.find(key);prior!=sectionRequests_.end()) {
      if(prior->second.target==request.target && request.payload.kind==CanvasControlPayloadKind::kSection &&
         prior->second.payload.section==request.payload.section && prior->second.deadlineSequence==request.deadlineSequence)
        return receipts_.at(key);
      auto collision=makeReceipt(key,CanvasControlReceiptState::kRejected,CanvasControlError::kInvalidValue);
      SectionOutcome outcome;outcome.failure=SectionFailure::kCollision;collision.section=outcome;
      return collision;
    }
    return makeReceipt(key, CanvasControlReceiptState::kDuplicate);
  }
  if (client == clientHighWater_.end() &&
      clientHighWater_.size() >= queueCapacity_ + receiptCapacity_) {
    return makeReceipt(key, CanvasControlReceiptState::kQueueFull,
                       CanvasControlError::kQueueFull, 0U, "client registry full");
  }
  const auto capture = isSelectionScoped(request.payload.kind)?port_.snapshot(request.target):CanvasControlSnapshot{};
  // Section carries explicit owned targets. Its producer lane does not read
  // mutable Runtime state; the owner rechecks lifecycle/context at execution.
  if (wasDestroyed(request.target) ||
      (request.payload.kind!=CanvasControlPayloadKind::kSection&&!port_.isTargetCurrent(request.target))) {
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
  if(request.payload.kind==CanvasControlPayloadKind::kSection &&
      static_cast<std::size_t>(std::count_if(receipts_.begin(),receipts_.end(),[](const auto& row){return row.second.state==CanvasControlReceiptState::kQueued;}))>=receiptCapacity_)
    return makeReceipt(key,CanvasControlReceiptState::kQueueFull,CanvasControlError::kQueueFull);
  if(request.payload.kind==CanvasControlPayloadKind::kSection)
    sectionRequests_.emplace(key,request);
  clientHighWater_[request.clientId] = request.requestId;
  queue_.push_back({key, request.target, request.deadlineSequence, request, {}});
  if (isSelectionScoped(request.payload.kind)) capturedSnapshots_[key] = capture;
  CanvasControlReceipt receipt = makeReceipt(key, CanvasControlReceiptState::kQueued);
  remember(receipt);
  return receipt;
}

CanvasControlReceipt CanvasControlService::enqueueMultiSelection(
    std::shared_ptr<MultiSelectionControlWork> request) {
  if (!request || !request->apply || !request->finish || request->key.clientId == 0 ||
      request->key.requestId == 0)
    return makeReceipt(request ? request->key : CanvasControlReceiptKey{},
                       CanvasControlReceiptState::kRejected, CanvasControlError::kInvalidValue);
  std::scoped_lock lock(mutex_);
  const auto key = request->key;
  const auto client = clientHighWater_.find(key.clientId);
  if (client != clientHighWater_.end() && key.requestId <= client->second)
    return makeReceipt(key, receipts_.contains(key) ? CanvasControlReceiptState::kDuplicate
                                                    : CanvasControlReceiptState::kExpired);
  const bool newClient = client == clientHighWater_.end();
  const auto previous = newClient ? 0U : client->second;
  if (newClient && clientHighWater_.size() >= queueCapacity_ + receiptCapacity_)
    return makeReceipt(key, CanvasControlReceiptState::kQueueFull, CanvasControlError::kQueueFull);
  const bool terminalReceiptRoom = static_cast<std::size_t>(std::count_if(receipts_.begin(), receipts_.end(), [](const auto& row) {
        return row.second.state == CanvasControlReceiptState::kQueued;
      })) < receiptCapacity_;
  const auto reject = [&](CanvasControlReceiptState state, CanvasControlError error) {
    auto receipt = makeReceipt(key, state, error);
    try {
      clientHighWater_[key.clientId] = key.requestId;
      if (terminalReceiptRoom) remember(receipt);
      return receipt;
    } catch (const std::bad_alloc&) {
      receipts_.erase(key);
      std::erase(receiptOrder_, key);
      if (newClient) clientHighWater_.erase(key.clientId);
      else clientHighWater_[key.clientId] = previous;
      return makeReceipt(key, CanvasControlReceiptState::kQueueFull, CanvasControlError::kQueueFull);
    }
  };
  if (wasDestroyed(request->target) || !port_.isTargetCurrent(request->target))
    return reject(CanvasControlReceiptState::kStaleTarget, CanvasControlError::kStaleTarget);
  if (queue_.size() >= queueCapacity_ || !terminalReceiptRoom)
    return reject(CanvasControlReceiptState::kQueueFull, CanvasControlError::kQueueFull);
  try {
    clientHighWater_[key.clientId] = key.requestId;
    queue_.push_back({key, request->target, request->deadlineSequence, {}, std::move(request)});
    auto receipt = makeReceipt(key, CanvasControlReceiptState::kQueued);
    remember(receipt);
    return receipt;
  } catch (const std::bad_alloc&) {
    std::erase_if(queue_, [&](const auto& pending) { return pending.key == key; });
    receipts_.erase(key);
    std::erase(receiptOrder_, key);
    if (newClient) clientHighWater_.erase(key.clientId);
    else clientHighWater_[key.clientId] = previous;
    return makeReceipt(key, CanvasControlReceiptState::kQueueFull, CanvasControlError::kQueueFull);
  }
}

std::size_t CanvasControlService::processPending(std::uint64_t ownerSequence,std::optional<CanvasControlReceiptKey> only) {
  std::size_t processed = 0;
  for (;;) {
    CanvasControlRequest request{};
    std::shared_ptr<MultiSelectionControlWork> multiSelection;
    std::optional<CanvasControlSnapshot> capturedSnapshot;
    {
      std::scoped_lock lock(mutex_);
      if (queue_.empty()) break;
      auto next=queue_.begin();
      if(only)next=std::find_if(queue_.begin(),queue_.end(),[&](const auto& item){return item.key==*only;});
      if(next==queue_.end())break;
      const auto key = next->key;
      const auto target = next->target;
      const auto deadline = next->deadlineSequence;
      if (next->request) request = *next->request;
      multiSelection = std::move(next->multiSelection);
      queue_.erase(next);
      const auto captured = capturedSnapshots_.find(key);
      if (captured != capturedSnapshots_.end()) capturedSnapshot = captured->second;
      if (wasDestroyed(target)) {
        remember(makeReceipt(key, CanvasControlReceiptState::kTargetDestroyed,
                             CanvasControlError::kStaleTarget));
        if (multiSelection) multiSelection->finish(CanvasControlReceiptState::kTargetDestroyed, CanvasControlError::kStaleTarget);
        capturedSnapshots_.erase(key);
        continue;
      }
      if (deadline != 0 && ownerSequence > deadline) {
        remember(makeReceipt(key, CanvasControlReceiptState::kExpired));
        if (multiSelection) multiSelection->finish(CanvasControlReceiptState::kExpired, CanvasControlError::kNone);
        capturedSnapshots_.erase(key);
        continue;
      }
      if (!port_.isTargetCurrent(target)) {
        remember(makeReceipt(key, CanvasControlReceiptState::kStaleTarget,
                             CanvasControlError::kStaleTarget));
        if (multiSelection) multiSelection->finish(CanvasControlReceiptState::kStaleTarget, CanvasControlError::kStaleTarget);
        capturedSnapshots_.erase(key);
        continue;
      }
      if (capturedSnapshot.has_value()) {
        const auto current = port_.snapshot(request.target);
        if (capturedSnapshot->selectedObject != current.selectedObject ||
            capturedSnapshot->selectionRevision != current.selectionRevision ||
            capturedSnapshot->semanticGeneration != current.semanticGeneration) {
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
          if (desired.strokeWidth == base.strokeWidth) {
            desired.strokeWidth = current.selectedAppearance.strokeWidth;
          }
          if (desired.opacity == base.opacity) desired.opacity = current.selectedAppearance.opacity;
          request.payload.appearance = desired;
        }
      }
    }
    if (multiSelection) {
      const auto error = multiSelection->apply();
      const auto state = error == CanvasControlError::kNone ? CanvasControlReceiptState::kApplied
          : error == CanvasControlError::kStaleTarget ? CanvasControlReceiptState::kStaleTarget
                                                      : CanvasControlReceiptState::kFailed;
      multiSelection->finish(state, error);
      {
        std::scoped_lock lock(mutex_);
        remember(makeReceipt(multiSelection->key, state, error));
      }
      ++processed;
      if (only) break;
      continue;
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
    receipt.section=result.section;
    receipt.appliedRevision = port_.snapshot(request.target).controlRevision;
    {
      std::scoped_lock lock(mutex_);
      capturedSnapshots_.erase(key);
      remember(std::move(receipt));
    }
    ++processed;
    if(only)break;
  }
  return processed;
}

void CanvasControlService::rejectPending(CanvasControlReceiptKey key,CanvasControlError error) {
  std::scoped_lock lock(mutex_);
  for (const auto& item : queue_)
    if (item.key == key && item.multiSelection)
      item.multiSelection->finish(CanvasControlReceiptState::kFailed, error);
  std::erase_if(queue_,[&](const auto& item){return item.key==key;});
  capturedSnapshots_.erase(key);
  remember(makeReceipt(key,CanvasControlReceiptState::kFailed,error));
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
      const auto key=it->key;
      if (it->multiSelection)
        it->multiSelection->finish(CanvasControlReceiptState::kTargetDestroyed, CanvasControlError::kStaleTarget);
      remember(makeReceipt(key,
                           CanvasControlReceiptState::kTargetDestroyed,
                           CanvasControlError::kStaleTarget));
      capturedSnapshots_.erase(key);
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
