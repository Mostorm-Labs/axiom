#include "canvas/debug_ui/command.hpp"

#include <algorithm>

namespace canvas::debug_ui {
BoundedCommandQueue::BoundedCommandQueue(std::size_t capacity, std::size_t receiptCapacity)
    : capacity_(capacity), receiptCapacity_(receiptCapacity) {}
std::optional<CommandReceipt> BoundedCommandQueue::admit(DebugCommand command) {
    std::lock_guard lock(mutex_);
    if (command.id == 0 || capacity_ == 0 || receiptCapacity_ == 0 || pending_.size() >= capacity_ || receipts_.contains(command.id)) return std::nullopt;
    if (receipts_.size() >= receiptCapacity_) {
        auto evict = std::find_if(receipts_.begin(), receipts_.end(), [](const auto& entry) {
            return entry.second.state != ReceiptState::kAccepted;
        });
        if (evict == receipts_.end()) return std::nullopt;
        evict->second.state = ReceiptState::kEvicted;
        receipts_.erase(evict);
    }
    pending_.push_back(std::move(command));
    auto receipt = CommandReceipt{pending_.back().id, ReceiptState::kAccepted, pending_.back().generation};
    receipts_.emplace(receipt.id, receipt);
    return receipt;
}
std::optional<DebugCommand> BoundedCommandQueue::take(std::uint64_t currentSequence, std::uint64_t currentGeneration) {
    std::lock_guard lock(mutex_);
    for (auto it = pending_.begin(); it != pending_.end();) {
        if (it->generation != currentGeneration) {
            terminalLocked(it->id, ReceiptState::kStaleGeneration);
            it = pending_.erase(it);
            continue;
        }
        if (it->deadlineSequence != 0 && it->deadlineSequence < currentSequence) {
            terminalLocked(it->id, ReceiptState::kExpired);
            it = pending_.erase(it);
            continue;
        }
        auto command = std::move(*it);
        pending_.erase(it);
        return command;
    }
    return std::nullopt;
}
std::optional<CommandReceipt> BoundedCommandQueue::terminalLocked(std::uint64_t id, ReceiptState state) {
    const auto it = receipts_.find(id);
    if (it == receipts_.end()) return std::nullopt;
    if (it->second.state != ReceiptState::kAccepted) return it->second;
    it->second.state = state;
    return it->second;
}
std::optional<CommandReceipt> BoundedCommandQueue::complete(std::uint64_t id, ReceiptState state) {
    std::lock_guard lock(mutex_);
    return terminalLocked(id, state);
}
std::size_t BoundedCommandQueue::destroyGeneration(std::uint64_t generation) {
    std::lock_guard lock(mutex_);
    std::size_t count = 0;
    for (auto it = pending_.begin(); it != pending_.end();) {
        if (it->generation == generation) {
            terminalLocked(it->id, ReceiptState::kDestroyed);
            it = pending_.erase(it);
            ++count;
        } else ++it;
    }
    return count;
}
std::size_t BoundedCommandQueue::expire(std::uint64_t currentSequence) {
    std::lock_guard lock(mutex_);
    std::size_t count = 0;
    for (auto it = pending_.begin(); it != pending_.end();) {
        if (it->deadlineSequence < currentSequence) {
            terminalLocked(it->id, ReceiptState::kExpired);
            it = pending_.erase(it);
            ++count;
        } else ++it;
    }
    return count;
}
std::size_t BoundedCommandQueue::size() const { std::lock_guard lock(mutex_); return pending_.size(); }
std::optional<CommandReceipt> BoundedCommandQueue::receipt(std::uint64_t id) const {
    std::lock_guard lock(mutex_);
    const auto it = receipts_.find(id);
    return it == receipts_.end() ? std::nullopt : std::optional<CommandReceipt>(it->second);
}
}  // namespace canvas::debug_ui
