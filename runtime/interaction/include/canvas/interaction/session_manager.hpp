#pragma once

#include "canvas/interaction/interaction_ports.hpp"
#include "canvas/interaction/interaction_dependency_footprint.hpp"

#include <cstddef>
#include <cstdint>

namespace canvas::interaction {

class InteractionSessionManager final {
  public:
    explicit InteractionSessionManager(TransientPresentationPort& transient) noexcept;
    [[nodiscard]] bool start(std::uint64_t sessionId) noexcept;
    [[nodiscard]] bool finish(std::uint64_t sessionId) noexcept;
    [[nodiscard]] bool cancel(std::uint64_t sessionId) noexcept;
    [[nodiscard]] bool trackDependency(std::uint64_t sessionId, foundation::ObjectId id) noexcept;
    [[nodiscard]] bool onChangeSet(std::uint64_t sessionId, const semantic::ChangeSet& changes,
                                   ConflictDecision* decision,
                                   CancellationReason* reason = nullptr) noexcept;
    [[nodiscard]] bool suspend(std::uint64_t sessionId) noexcept;
    [[nodiscard]] bool sourceLost(std::uint64_t sessionId) noexcept;
    void cancelAll(CancellationReason reason = CancellationReason::kNone) noexcept;
    [[nodiscard]] std::size_t activeCount() const noexcept { return activeCount_; }
    [[nodiscard]] CancellationReason lastCancellationReason() const noexcept { return cancellationReason_; }

  private:
    TransientPresentationPort& transient_;
    std::uint64_t activeSession_ = 0;
    std::size_t activeCount_ = 0;
    InteractionDependencyFootprint footprint_;
    CancellationReason cancellationReason_ = CancellationReason::kNone;
};

} // namespace canvas::interaction
