#include "canvas/scene/scene_binding.hpp"

#include <utility>

namespace canvas {

foundation::Result<SceneSyncReceipt> SceneBinding::rebuild(const ICompiledSceneSource& source) {
    auto snapshotResult = source.compileFull();
    if (!snapshotResult) {
        return foundation::Result<SceneSyncReceipt>::failure(snapshotResult.error());
    }
    auto applyResult = _scene.replace(std::move(snapshotResult.value()));
    if (!applyResult) {
        return foundation::Result<SceneSyncReceipt>::failure(applyResult.error());
    }
    SceneApplyReceipt apply = std::move(applyResult.value());
    return foundation::Result<SceneSyncReceipt>::success(SceneSyncReceipt{
        .revision = apply.afterRevision,
        .semanticGeneration = {},
        .disposition = SceneSyncDisposition::kRebuiltFull,
        .apply = std::move(apply),
        .incrementalFailure = std::nullopt,
    });
}

foundation::Result<SceneSyncReceipt> SceneBinding::synchronize(const ICompiledSceneSource& source) {
    auto deltaResult = source.compileDelta();
    if (!deltaResult) {
        if (deltaResult.error().code != foundation::ErrorCode::kRequiresFullRebuild) {
            return foundation::Result<SceneSyncReceipt>::failure(deltaResult.error());
        }
        return rebuildAfterIncrementalFailure(source, deltaResult.error());
    }

    auto applyResult = _scene.apply(std::move(deltaResult.value()));
    if (!applyResult) {
        if (applyResult.error().code != foundation::ErrorCode::kRequiresFullRebuild) {
            return foundation::Result<SceneSyncReceipt>::failure(applyResult.error());
        }
        return rebuildAfterIncrementalFailure(source, applyResult.error());
    }

    SceneApplyReceipt apply = std::move(applyResult.value());
    return foundation::Result<SceneSyncReceipt>::success(SceneSyncReceipt{
        .revision = apply.afterRevision,
        .semanticGeneration = {},
        .disposition = SceneSyncDisposition::kAppliedIncremental,
        .apply = std::move(apply),
        .incrementalFailure = std::nullopt,
    });
}

foundation::Result<SceneSyncReceipt> SceneBinding::rebuild(
    const ISemanticSceneCompiler& compiler, const SceneCommitInput& input) {
    if (input.changes != nullptr) {
        return foundation::Result<SceneSyncReceipt>::failure(
            foundation::Error{foundation::ErrorCode::kInvalidRevision,
                              "Full semantic rebuild must not carry a ChangeSet"});
    }
    auto snapshot = compiler.compileFull(input.post_state);
    if (!snapshot) {
        return foundation::Result<SceneSyncReceipt>::failure(snapshot.error());
    }
    auto applied = _scene.replace(input, std::move(snapshot.value()));
    if (!applied) {
        return foundation::Result<SceneSyncReceipt>::failure(applied.error());
    }
    SceneApplyReceipt receipt = std::move(applied.value());
    return foundation::Result<SceneSyncReceipt>::success(SceneSyncReceipt{
        .revision = receipt.afterRevision,
        .semanticGeneration = input.after_generation,
        .disposition = SceneSyncDisposition::kRebuiltFull,
        .apply = std::move(receipt),
        .incrementalFailure = std::nullopt,
    });
}

foundation::Result<SceneSyncReceipt> SceneBinding::synchronize(
    const ISemanticSceneCompiler& compiler, const SceneCommitInput& input) {
    if (input.changes == nullptr) {
        return foundation::Result<SceneSyncReceipt>::failure(
            foundation::Error{foundation::ErrorCode::kInvalidRevision,
                              "Incremental semantic synchronization requires a ChangeSet"});
    }
    auto delta = compiler.compileDelta(input.post_state, *input.changes);
    if (!delta) {
        return foundation::Result<SceneSyncReceipt>::failure(delta.error());
    }
    auto applied = _scene.apply(input, std::move(delta.value()));
    if (!applied) {
        return foundation::Result<SceneSyncReceipt>::failure(applied.error());
    }
    SceneApplyReceipt receipt = std::move(applied.value());
    return foundation::Result<SceneSyncReceipt>::success(SceneSyncReceipt{
        .revision = receipt.afterRevision,
        .semanticGeneration = input.after_generation,
        .disposition = SceneSyncDisposition::kAppliedIncremental,
        .apply = std::move(receipt),
        .incrementalFailure = std::nullopt,
    });
}

foundation::Result<SceneSyncReceipt>
SceneBinding::rebuildAfterIncrementalFailure(const ICompiledSceneSource& source,
                                             foundation::Error incrementalFailure) {
    auto snapshotResult = source.compileFull();
    if (!snapshotResult) {
        return foundation::Result<SceneSyncReceipt>::failure(snapshotResult.error());
    }
    auto applyResult = _scene.replace(std::move(snapshotResult.value()));
    if (!applyResult) {
        return foundation::Result<SceneSyncReceipt>::failure(applyResult.error());
    }
    SceneApplyReceipt apply = std::move(applyResult.value());
    return foundation::Result<SceneSyncReceipt>::success(SceneSyncReceipt{
        .revision = apply.afterRevision,
        .semanticGeneration = {},
        .disposition = SceneSyncDisposition::kRebuiltFull,
        .apply = std::move(apply),
        .incrementalFailure = std::move(incrementalFailure),
    });
}

} // namespace canvas
