#include "canvas/interaction/session_manager.hpp"

namespace canvas::interaction {

InteractionSessionManager::InteractionSessionManager(TransientPresentationPort& transient) noexcept
    : transient_(transient) {}

bool InteractionSessionManager::start(std::uint64_t sessionId) noexcept {
    if (activeCount_ != 0 || sessionId == 0) return false;
    activeSession_ = sessionId;
    activeCount_ = 1;
    footprint_.clear();
    cancellationReason_ = CancellationReason::kNone;
    return true;
}

bool InteractionSessionManager::start(const input::PointerKey& key, std::uint64_t sessionId) noexcept {
    if (!key.valid() || sessionId == 0 || keyedSessions_.contains(key)) return false;
    keyedSessions_.emplace(key, sessionId);
    return true;
}

bool InteractionSessionManager::finish(const input::PointerKey& key, std::uint64_t sessionId) noexcept {
    const auto it = keyedSessions_.find(key);
    if (it == keyedSessions_.end() || it->second != sessionId) return false;
    keyedSessions_.erase(it);
    return true;
}

bool InteractionSessionManager::cancel(const input::PointerKey& key, std::uint64_t sessionId) noexcept {
    if (!finish(key, sessionId)) return false;
    transient_.cancel(sessionId);
    return true;
}

bool InteractionSessionManager::finish(std::uint64_t sessionId) noexcept {
    if (activeCount_ == 0 || activeSession_ != sessionId) return false;
    activeSession_ = 0;
    activeCount_ = 0;
    footprint_.clear();
    return true;
}

bool InteractionSessionManager::cancel(std::uint64_t sessionId) noexcept {
    if (!finish(sessionId)) return false;
    transient_.cancel(sessionId);
    return true;
}

bool InteractionSessionManager::trackDependency(std::uint64_t sessionId, foundation::ObjectId id) noexcept {
    return activeCount_ != 0 && activeSession_ == sessionId && footprint_.track(id);
}
bool InteractionSessionManager::onChangeSet(std::uint64_t sessionId, const semantic::ChangeSet& changes,
                                            ConflictDecision* decision, CancellationReason* reason) noexcept {
    if (decision == nullptr || activeCount_ == 0 || activeSession_ != sessionId) return false;
    *decision = footprint_.evaluate(changes, reason);
    if (*decision == ConflictDecision::kCancel) {
        cancellationReason_ = CancellationReason::kTargetDeleted;
        return cancel(sessionId);
    }
    return true;
}
bool InteractionSessionManager::suspend(std::uint64_t sessionId) noexcept {
    if (activeCount_ == 0 || activeSession_ != sessionId) return false;
    cancellationReason_ = CancellationReason::kSurfaceSuspended;
    return cancel(sessionId);
}
bool InteractionSessionManager::sourceLost(std::uint64_t sessionId) noexcept {
    if (activeCount_ == 0 || activeSession_ != sessionId) return false;
    cancellationReason_ = CancellationReason::kSourceLost;
    return cancel(sessionId);
}

void InteractionSessionManager::cancelAll(CancellationReason reason) noexcept {
    if (activeCount_ != 0) {
        cancellationReason_ = reason;
        static_cast<void>(cancel(activeSession_));
    }
}

void InteractionSessionManager::cancelAllKeyed(CancellationReason reason) noexcept {
    cancellationReason_ = reason;
    for (const auto& [key, sessionId] : keyedSessions_) {
        static_cast<void>(key);
        transient_.cancel(sessionId);
    }
    keyedSessions_.clear();
}

} // namespace canvas::interaction
