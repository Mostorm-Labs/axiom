#include "canvas/scene/incremental_runtime_coordinator.hpp"

#include "incremental_runtime_full_materialization_bridge.hpp"

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
    auto runtimePrepared = runtimeScene_.prepare(input.post_state);
    if (!runtimePrepared) {
        return foundation::Result<SceneSyncReceipt>::failure(runtimePrepared.error());
    }
    if (checkpointFails(RuntimeCheckpoint::kBeforeRuntimePrepare) ||
        checkpointFails(RuntimeCheckpoint::kAfterRuntimePrepare)) {
        return foundation::Result<SceneSyncReceipt>::failure(
            {foundation::ErrorCode::kParticipantRejected, "RuntimeScene checkpoint failure"});
    }
    // The coordinator boundary is the last point at which all participants
    // are still unpublished.  A deterministic failure here must therefore
    // precede SceneBinding's participant prepare/commit transaction.
    if (checkpointFails(RuntimeCheckpoint::kBeforePublication)) {
        return foundation::Result<SceneSyncReceipt>::failure(
            {foundation::ErrorCode::kParticipantRejected, "Publication checkpoint failure"});
    }
    auto incremental = binding_.synchronize(compiler, input);
    if (incremental || incremental.error().code != foundation::ErrorCode::kRequiresFullRebuild) {
        if (!incremental) return incremental;
        if (runtimePrepared.value().projection.generation != input.post_state.generation()) {
            return foundation::Result<SceneSyncReceipt>::failure(
                {foundation::ErrorCode::kInvalidRevision,
                 "RuntimeScene prepared generation does not match semantic input"});
        }
        runtimeScene_.publish(std::move(runtimePrepared.value()));
        return incremental;
    }

    // An unsafe/unsupported incremental continuation is explicitly recovered
    // through the independent full compiler path. The recovery receipt is
    // distinguishable and never reported as incremental success.
    const SceneCommitInput recoveryInput(input.after_generation, input.post_state);
    auto fullMaterialized = internal::materializeFullScene(input.post_state);
    if (!fullMaterialized) {
        return foundation::Result<SceneSyncReceipt>::failure(fullMaterialized.error());
    }
    RuntimeSceneProjection recoveryProjection;
    recoveryProjection.generation = fullMaterialized.value().generation;
    recoveryProjection.records.reserve(fullMaterialized.value().records.size());
    for (auto& record : fullMaterialized.value().records) {
        recoveryProjection.records.push_back(RuntimeSceneRecord{
            .objectId = record.objectId,
            .kind = record.kind,
            .kindVersion = record.kindVersion,
            .placement = std::move(record.placement),
            .transform = std::move(record.transform),
            .properties = std::move(record.properties),
            .content = std::move(record.content),
            .eraseMasks = std::move(record.eraseMasks),
            .geometryBounds = record.geometryBounds,
            .visualBounds = record.visualBounds,
            .worldBounds = record.worldBounds,
            .referenceGeometryDigest = std::move(record.referenceGeometryDigest),
            .directDependencies = std::move(record.directDependencies),
        });
    }
    auto recoveryPrepared = runtimeScene_.prepare(std::move(recoveryProjection));
    if (!recoveryPrepared) {
        return foundation::Result<SceneSyncReceipt>::failure(recoveryPrepared.error());
    }
    auto recovered = binding_.rebuild(compiler, recoveryInput);
    if (!recovered) {
        return recovered;
    }
    runtimeScene_.publish(std::move(recoveryPrepared.value()));
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
    auto materialized = internal::materializeFullScene(input.post_state);
    if (!materialized) {
        return foundation::Result<SceneSyncReceipt>::failure(materialized.error());
    }
    RuntimeSceneProjection projection;
    projection.generation = materialized.value().generation;
    projection.records.reserve(materialized.value().records.size());
    for (auto& record : materialized.value().records) {
        projection.records.push_back(RuntimeSceneRecord{
            .objectId = record.objectId,
            .kind = record.kind,
            .kindVersion = record.kindVersion,
            .placement = std::move(record.placement),
            .transform = std::move(record.transform),
            .properties = std::move(record.properties),
            .content = std::move(record.content),
            .eraseMasks = std::move(record.eraseMasks),
            .geometryBounds = record.geometryBounds,
            .visualBounds = record.visualBounds,
            .worldBounds = record.worldBounds,
            .referenceGeometryDigest = std::move(record.referenceGeometryDigest),
            .directDependencies = std::move(record.directDependencies),
        });
    }
    auto runtimePrepared = runtimeScene_.prepare(std::move(projection));
    if (!runtimePrepared) {
        return foundation::Result<SceneSyncReceipt>::failure(runtimePrepared.error());
    }
    if (checkpointFails(RuntimeCheckpoint::kBeforePublication)) {
        return foundation::Result<SceneSyncReceipt>::failure(
            {foundation::ErrorCode::kParticipantRejected, "Recovery publication checkpoint failure"});
    }
    auto result = binding_.rebuild(compiler, input);
    if (result) runtimeScene_.publish(std::move(runtimePrepared.value()));
    return result;
}

} // namespace canvas
