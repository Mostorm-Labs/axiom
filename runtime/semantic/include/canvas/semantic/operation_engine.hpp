#pragma once

#include "canvas/semantic/apply_plan.hpp"
#include "canvas/semantic/apply_source.hpp"
#include "canvas/semantic/canonical_commit_clock.hpp"
#include "canvas/semantic/canonical_commit_record.hpp"
#include "canvas/semantic/change_set.hpp"
#include "canvas/semantic/semantic_generation.hpp"

#include <cstdint>
#include <optional>

#if defined(AXIOM_FC08_QUALIFICATION_HOOKS)
#include <memory>
namespace canvas::qualification { struct State; class Access; }
#endif

namespace canvas::semantic {

class AppliedOperationLedger;
class IndexedObjectStore;
class ReferenceObjectStore;
class StagedObjectView;

// Serialized precommit admission. A local command reserves its real inverse
// and History entry here; canonical/replay callers keep the ordinary path.
class OperationAdmission {
  public:
    virtual ~OperationAdmission() = default;
    [[nodiscard]] virtual bool admit(const PreparedApplyPlan&,
        const StatefulValidationContext&, const StagedObjectView&) = 0;
};

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
#if defined(AXIOM_FC08_QUALIFICATION_HOOKS)
    friend class canvas::qualification::Access;
    std::shared_ptr<canvas::qualification::State> fc08Qualification_;
#endif
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
        CanonicalCommitClock& canonical_commit_clock,
        OperationAdmission* admission = nullptr) const;

    [[nodiscard]] ApplyResult apply(
        const Operation& operation,
        ApplySource source,
        IndexedObjectStore& objects,
        AppliedOperationLedger& applied_operations,
        SemanticGenerationState& generation,
        CanonicalCommitClock& canonical_commit_clock,
        OperationAdmission* admission = nullptr) const;

    [[nodiscard]] ApplyResult apply(
        const Operation& operation,
        ApplySource source,
        ReferenceObjectStore& objects,
        ResourceManifest& manifest,
        AppliedOperationLedger& applied_operations,
        SemanticGenerationState& generation,
        CanonicalCommitClock& canonical_commit_clock,
        OperationAdmission* admission = nullptr) const;

    [[nodiscard]] ApplyResult apply(
        const Operation& operation,
        ApplySource source,
        IndexedObjectStore& objects,
        ResourceManifest& manifest,
        AppliedOperationLedger& applied_operations,
        SemanticGenerationState& generation,
        CanonicalCommitClock& canonical_commit_clock,
        OperationAdmission* admission = nullptr) const;
};

} // namespace canvas::semantic
