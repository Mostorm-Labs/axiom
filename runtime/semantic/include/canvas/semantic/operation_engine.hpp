#pragma once

#include "canvas/semantic/apply_plan.hpp"
#include "canvas/semantic/apply_source.hpp"
#include "canvas/semantic/canonical_commit_clock.hpp"
#include "canvas/semantic/canonical_commit_record.hpp"
#include "canvas/semantic/change_set.hpp"
#include "canvas/semantic/semantic_generation.hpp"

#include <cstdint>
#include <optional>

namespace canvas::semantic {

class AppliedOperationLedger;
class IndexedObjectStore;
class ReferenceObjectStore;

enum class ApplyDisposition : std::uint8_t {
    kApplied = 0,
    kAlreadyApplied,
    kRejected,
    kCommitBlocked,
};

enum class CommitBlockReason : std::uint8_t {
    kNone = 0,
    kInvalidRuntimeEpoch,
    kCommitLaneExhausted,
};

struct ApplyResult final {
    ApplyDisposition disposition = ApplyDisposition::kRejected;
    StatefulResult error{};
    CommitBlockReason commit_block_reason = CommitBlockReason::kNone;
    std::optional<CanonicalCommitRecord> commit_record;
};

class OperationEngine final {
  public:
    [[nodiscard]] PrepareResult prepare(
        const Operation& operation,
        const StatefulValidationContext& context) const;

    [[nodiscard]] ApplyResult apply(
        const Operation& operation,
        ApplySource source,
        ReferenceObjectStore& objects,
        AppliedOperationLedger& applied_operations,
        SemanticGenerationState& generation,
        CanonicalCommitClock& canonical_commit_clock) const;

    [[nodiscard]] ApplyResult apply(
        const Operation& operation,
        ApplySource source,
        IndexedObjectStore& objects,
        AppliedOperationLedger& applied_operations,
        SemanticGenerationState& generation,
        CanonicalCommitClock& canonical_commit_clock) const;
};

} // namespace canvas::semantic
