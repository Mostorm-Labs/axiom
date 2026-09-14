#include "canvas/scene/incremental_runtime_coordinator.hpp"

#include <algorithm>

namespace canvas {

foundation::Result<RuntimeUpdatePlan> IncrementalRuntimeCoordinator::plan(
    const semantic::SemanticReadView& postState,
    const semantic::ChangeSet& changes) const {
    if (changes.beforeGeneration() >= changes.afterGeneration() ||
        postState.generation() != changes.afterGeneration()) {
        return foundation::Result<RuntimeUpdatePlan>::failure(
            {foundation::ErrorCode::kInvalidRevision,
             "Runtime update generations do not form a valid successor"});
    }
    RuntimeUpdatePlan result{
        .beforeGeneration = changes.beforeGeneration(),
        .afterGeneration = changes.afterGeneration(),
        .disposition = RuntimeUpdateDisposition::kIncremental,
        .requiresFullRebuild = false,
        .affectedObjects = {},
    };
    result.affectedObjects.reserve(changes.objects().size());
    for (const auto& change : changes.objects()) {
        result.affectedObjects.push_back(change.object_id);
    }
    std::sort(result.affectedObjects.begin(), result.affectedObjects.end());
    result.affectedObjects.erase(
        std::unique(result.affectedObjects.begin(), result.affectedObjects.end()),
        result.affectedObjects.end());
    return foundation::Result<RuntimeUpdatePlan>::success(std::move(result));
}

foundation::Result<SceneSyncReceipt> IncrementalRuntimeCoordinator::apply(
    const ISemanticSceneCompiler& compiler,
    const SceneCommitInput& input) {
    if (input.changes == nullptr) {
        return foundation::Result<SceneSyncReceipt>::failure(
            {foundation::ErrorCode::kInvalidRevision,
             "Incremental runtime update requires a ChangeSet"});
    }
    const auto updatePlan = plan(input.post_state, *input.changes);
    if (!updatePlan) {
        return foundation::Result<SceneSyncReceipt>::failure(updatePlan.error());
    }
    auto incremental = binding_.synchronize(compiler, input);
    if (incremental || incremental.error().code != foundation::ErrorCode::kRequiresFullRebuild) {
        return incremental;
    }

    // An unsafe/unsupported incremental continuation is explicitly recovered
    // through the independent full compiler path. The recovery receipt is
    // distinguishable and never reported as incremental success.
    const SceneCommitInput recoveryInput(input.after_generation, input.post_state);
    auto recovered = binding_.rebuild(compiler, recoveryInput);
    if (!recovered) {
        return recovered;
    }
    auto receipt = std::move(recovered.value());
    receipt.incrementalFailure = incremental.error();
    return foundation::Result<SceneSyncReceipt>::success(std::move(receipt));
}

foundation::Result<SceneSyncReceipt> IncrementalRuntimeCoordinator::recover(
    const ISemanticSceneCompiler& compiler,
    const SceneCommitInput& input) {
    if (input.changes != nullptr) {
        return foundation::Result<SceneSyncReceipt>::failure(
            {foundation::ErrorCode::kInvalidRevision,
             "Full runtime recovery must not carry a ChangeSet"});
    }
    return binding_.rebuild(compiler, input);
}

} // namespace canvas
