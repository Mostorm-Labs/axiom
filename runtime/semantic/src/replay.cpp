#include "canvas/semantic/replay.hpp"

namespace canvas::semantic {
namespace {

template <typename Store>
ReplayResult replayAndFinalizeImpl(
    std::span<const Operation> continuation,
    DocumentRuntimeState& state,
    Store& objects,
    AppliedOperationLedger& applied_operations,
    SemanticGenerationState& generation,
    CanonicalCommitClock& commit_clock) {
    ReplayResult result;
    if (state != DocumentRuntimeState::kLoading) {
        return result;
    }

    OperationEngine engine;
    for (std::size_t index = 0U; index < continuation.size(); ++index) {
        const Operation& operation = continuation[index];
        const ApplyResult applied = engine.apply(
            operation,
            ApplySource::kRestoreReplay,
            objects,
            applied_operations,
            generation,
            commit_clock);
        if (applied.disposition == ApplyDisposition::kApplied) {
            ++result.applied;
            continue;
        }
        if (applied.disposition == ApplyDisposition::kAlreadyApplied) {
            ++result.already_applied;
            continue;
        }

        state = DocumentRuntimeState::kFailed;
        result.failure = ReplayFailure{index, operation.id, applied.disposition};
        return result;
    }

    state = DocumentRuntimeState::kReady;
    result.ready = true;
    return result;
}

} // namespace

ReplayResult ReplayCoordinator::replayAndFinalize(
    std::span<const Operation> continuation,
    DocumentRuntimeState& state,
    ReferenceObjectStore& objects,
    AppliedOperationLedger& applied_operations,
    SemanticGenerationState& generation,
    CanonicalCommitClock& commit_clock) {
    return replayAndFinalizeImpl(
        continuation, state, objects, applied_operations, generation, commit_clock);
}

ReplayResult ReplayCoordinator::replayAndFinalize(
    std::span<const Operation> continuation,
    DocumentRuntimeState& state,
    IndexedObjectStore& objects,
    AppliedOperationLedger& applied_operations,
    SemanticGenerationState& generation,
    CanonicalCommitClock& commit_clock) {
    return replayAndFinalizeImpl(
        continuation, state, objects, applied_operations, generation, commit_clock);
}

} // namespace canvas::semantic
