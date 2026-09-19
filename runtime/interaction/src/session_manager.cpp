#include "canvas/interaction/session_manager.hpp"

namespace canvas::interaction {

InteractionSessionManager::InteractionSessionManager(TransientPresentationPort& transient) noexcept
    : transient_(transient) {}

bool InteractionSessionManager::start(std::uint64_t sessionId) noexcept {
    if (activeCount_ != 0 || sessionId == 0) return false;
    activeSession_ = sessionId;
    activeCount_ = 1;
    return true;
}

bool InteractionSessionManager::finish(std::uint64_t sessionId) noexcept {
    if (activeCount_ == 0 || activeSession_ != sessionId) return false;
    activeSession_ = 0;
    activeCount_ = 0;
    return true;
}

bool InteractionSessionManager::cancel(std::uint64_t sessionId) noexcept {
    if (!finish(sessionId)) return false;
    transient_.cancel(sessionId);
    return true;
}

void InteractionSessionManager::cancelAll() noexcept {
    if (activeCount_ != 0) static_cast<void>(cancel(activeSession_));
}

} // namespace canvas::interaction
