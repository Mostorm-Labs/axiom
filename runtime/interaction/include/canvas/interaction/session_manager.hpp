#pragma once

#include "canvas/interaction/interaction_ports.hpp"
#include "canvas/input/pointer_key.hpp"
#include "canvas/interaction/interaction_dependency_footprint.hpp"

#include <cstddef>
#include <cstdint>
#include <unordered_map>

namespace canvas::interaction {

class InteractionSessionManager final {
  public:
    explicit InteractionSessionManager(TransientPresentationPort& transient) noexcept;
    [[nodiscard]] bool start(std::uint64_t sessionId) noexcept;
    [[nodiscard]] bool start(const input::PointerKey& key, std::uint64_t sessionId) noexcept;
    [[nodiscard]] bool finish(const input::PointerKey& key, std::uint64_t sessionId) noexcept;
    [[nodiscard]] bool cancel(const input::PointerKey& key, std::uint64_t sessionId) noexcept;
    [[nodiscard]] bool finish(std::uint64_t sessionId) noexcept;
    [[nodiscard]] bool cancel(std::uint64_t sessionId) noexcept;
    [[nodiscard]] bool trackDependency(std::uint64_t sessionId, foundation::ObjectId id) noexcept;
    [[nodiscard]] bool onChangeSet(std::uint64_t sessionId, const semantic::ChangeSet& changes,
                                   ConflictDecision* decision,
                                   CancellationReason* reason = nullptr) noexcept;
    [[nodiscard]] bool suspend(std::uint64_t sessionId) noexcept;
    [[nodiscard]] bool sourceLost(std::uint64_t sessionId) noexcept;
    void cancelAll(CancellationReason reason = CancellationReason::kNone) noexcept;
    void cancelAllKeyed(CancellationReason reason = CancellationReason::kNone) noexcept;
    [[nodiscard]] std::size_t activeCount() const noexcept { return activeCount_; }
    [[nodiscard]] std::size_t keyedActiveCount() const noexcept { return keyedSessions_.size(); }
    [[nodiscard]] CancellationReason lastCancellationReason() const noexcept { return cancellationReason_; }

  private:
    TransientPresentationPort& transient_;
    std::uint64_t activeSession_ = 0;
    std::size_t activeCount_ = 0;
    InteractionDependencyFootprint footprint_;
    CancellationReason cancellationReason_ = CancellationReason::kNone;
    std::unordered_map<input::PointerKey, std::uint64_t, input::PointerKeyHash> keyedSessions_;
};

} // namespace canvas::interaction
