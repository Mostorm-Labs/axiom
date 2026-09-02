#include "canvas/semantic/canonical_commit_clock.hpp"
#include "canvas/semantic/canonical_commit_record.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <limits>
#include <type_traits>
#include <utility>

namespace canvas::semantic::internal {

// The production clock deliberately has no general advancement API. This
// test-only friend verifies the candidate/commit split that the OperationEngine
// uses on the ordered canonical write lane.
class CanonicalCommitClockTestAccess final {
  public:
    [[nodiscard]] static bool prepareSuccessor(
        const CanonicalCommitClock& clock,
        CommitOrdinal& out) noexcept {
        return clock.prepareSuccessor(out);
    }

    static void commitSuccessor(
        CanonicalCommitClock& clock,
        CommitOrdinal successor) noexcept {
        clock.commitSuccessor(successor);
    }
};

} // namespace canvas::semantic::internal

namespace canvas::semantic {
namespace {

TEST(CanonicalCommitClock, CC_A3_01_StrongOrdinalReservesZeroAndSeparatesNamespaces) {
    static_assert(!std::is_convertible_v<CommitOrdinal, std::uint64_t>);
    static_assert(!std::is_convertible_v<std::uint64_t, CommitOrdinal>);
    static_assert(!std::is_convertible_v<CommitOrdinal, SemanticGeneration>);
    static_assert(!std::is_convertible_v<SemanticGeneration, CommitOrdinal>);

    const CommitOrdinal reserved{};
    const CommitOrdinal first(1U);
    EXPECT_EQ(reserved.value(), 0U);
    EXPECT_LT(reserved, first);

    const CanonicalCommitStamp first_stamp{RuntimeEpoch(7U), first};
    EXPECT_EQ(first_stamp.runtime_epoch.value(), 7U);
    EXPECT_EQ(first_stamp.ordinal.value(), 1U);
}

TEST(CanonicalCommitClock, CC_A3_05_CandidateDoesNotConsumeUntilCommitted) {
    CanonicalCommitClock clock(RuntimeEpoch(7U));
    CommitOrdinal candidate{};

    ASSERT_TRUE(internal::CanonicalCommitClockTestAccess::prepareSuccessor(clock, candidate));
    EXPECT_EQ(candidate, CommitOrdinal(1U));
    EXPECT_EQ(clock.lastCommittedOrdinal(), CommitOrdinal{});

    CommitOrdinal reusable_candidate{};
    ASSERT_TRUE(internal::CanonicalCommitClockTestAccess::prepareSuccessor(clock, reusable_candidate));
    EXPECT_EQ(reusable_candidate, candidate);
    EXPECT_EQ(clock.lastCommittedOrdinal(), CommitOrdinal{});

    internal::CanonicalCommitClockTestAccess::commitSuccessor(clock, candidate);
    EXPECT_EQ(clock.lastCommittedOrdinal(), CommitOrdinal(1U));
}

TEST(CanonicalCommitClock, CC_A3_06_And_07_CheckedRangeReachesMaximumWithoutWrap) {
    CanonicalCommitClock clock(
        RuntimeEpoch(9U), CommitOrdinal(std::numeric_limits<std::uint64_t>::max() - 1U));
    CommitOrdinal maximum{};

    ASSERT_TRUE(internal::CanonicalCommitClockTestAccess::prepareSuccessor(clock, maximum));
    EXPECT_EQ(maximum, CommitOrdinal(std::numeric_limits<std::uint64_t>::max()));
    internal::CanonicalCommitClockTestAccess::commitSuccessor(clock, maximum);
    EXPECT_EQ(clock.lastCommittedOrdinal(), maximum);

    CommitOrdinal overflow_candidate{};
    EXPECT_FALSE(internal::CanonicalCommitClockTestAccess::prepareSuccessor(clock, overflow_candidate));
    EXPECT_EQ(clock.lastCommittedOrdinal(), maximum);
}

TEST(CanonicalCommitClock, CC_A3_12_RecordConstructionUsesNothrowMovablePreparedFacts) {
    static_assert(std::is_nothrow_move_constructible_v<ChangeSet>);
    static_assert(std::is_nothrow_move_constructible_v<CanonicalCommitRecord>);

    ChangeSet changes = ChangeSet::fromChanges(
        SemanticGeneration(3U),
        SemanticGeneration(4U),
        {{ObjectId::fromUint64(1U), SemanticChangeFlags::kCreated, {}}});
    const CanonicalCommitRecord record{
        OperationId{ObjectId::fromUint64(99U)},
        ApplySource::kLocalCommand,
        SemanticGeneration(3U),
        SemanticGeneration(4U),
        CanonicalCommitStamp{RuntimeEpoch(7U), CommitOrdinal(12U)},
        std::move(changes)};
    const CanonicalCommitRecord copied = record;
    CanonicalCommitRecord moved = std::move(copied);

    EXPECT_EQ(moved.operation_id, record.operation_id);
    EXPECT_EQ(moved.commit_stamp, record.commit_stamp);
    EXPECT_EQ(moved.change_set.beforeGeneration(), SemanticGeneration(3U));
    EXPECT_EQ(moved.change_set.afterGeneration(), SemanticGeneration(4U));
}

} // namespace
} // namespace canvas::semantic
