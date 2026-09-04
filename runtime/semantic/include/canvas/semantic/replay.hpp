#pragma once

#include "canvas/semantic/applied_operation_ledger.hpp"
#include "canvas/semantic/canonical_commit_clock.hpp"
#include "canvas/semantic/document_runtime_state.hpp"
#include "canvas/semantic/indexed_object_store.hpp"
#include "canvas/semantic/operation_engine.hpp"
#include "canvas/semantic/reference_object_store.hpp"
#include "canvas/semantic/semantic_generation.hpp"

#include <cstddef>
#include <optional>
#include <span>

namespace canvas::semantic {

struct ReplayFailure final {
    std::size_t operation_index = 0U;
    OperationId operation_id{};
    ApplyDisposition disposition = ApplyDisposition::kRejected;
};

struct ReplayResult final {
    bool ready = false;
    std::size_t applied = 0U;
    std::size_t already_applied = 0U;
    std::optional<ReplayFailure> failure;
};

class ReplayCoordinator final {
  public:
    [[nodiscard]] static ReplayResult replayAndFinalize(
        std::span<const Operation> continuation,
        DocumentRuntimeState& state,
        ReferenceObjectStore& objects,
        AppliedOperationLedger& applied_operations,
        SemanticGenerationState& generation,
        CanonicalCommitClock& commit_clock);

    [[nodiscard]] static ReplayResult replayAndFinalize(
        std::span<const Operation> continuation,
        DocumentRuntimeState& state,
        IndexedObjectStore& objects,
        AppliedOperationLedger& applied_operations,
        SemanticGenerationState& generation,
        CanonicalCommitClock& commit_clock);
};

} // namespace canvas::semantic
