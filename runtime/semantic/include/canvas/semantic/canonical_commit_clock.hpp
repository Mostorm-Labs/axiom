#pragma once

#include "canvas/semantic/canonical_commit_stamp.hpp"

#include <cstdint>
#include <limits>

namespace canvas::semantic {

class OperationEngine;
namespace internal {
class CanonicalCommitClockTestAccess;
}

// Mutable state for one loaded RuntimeEpoch's ordered canonical write lane.
// It is intentionally supplied to the stateless OperationEngine by its
// owner; no global clock or public advancement operation exists.
class CanonicalCommitClock final {
  public:
    explicit constexpr CanonicalCommitClock(
        RuntimeEpoch runtime_epoch = RuntimeEpoch{},
        CommitOrdinal baseline = CommitOrdinal{}) noexcept
        : runtime_epoch_(runtime_epoch), last_committed_ordinal_(baseline) {}

    [[nodiscard]] constexpr RuntimeEpoch runtimeEpoch() const noexcept {
        return runtime_epoch_;
    }

    [[nodiscard]] constexpr CommitOrdinal lastCommittedOrdinal() const noexcept {
        return last_committed_ordinal_;
    }

  private:
    friend class OperationEngine;
    friend class internal::CanonicalCommitClockTestAccess;

    [[nodiscard]] constexpr bool prepareSuccessor(CommitOrdinal& out) const noexcept {
        if (last_committed_ordinal_.value() == std::numeric_limits<std::uint64_t>::max()) {
            return false;
        }
        out = CommitOrdinal(last_committed_ordinal_.value() + 1U);
        return true;
    }

    constexpr void commitSuccessor(CommitOrdinal successor) noexcept {
        last_committed_ordinal_ = successor;
    }

    RuntimeEpoch runtime_epoch_{};
    CommitOrdinal last_committed_ordinal_{};
};

} // namespace canvas::semantic
