#include "canvas/scene/incremental_runtime_coordinator.hpp"

#include "incremental_runtime_full_materialization_bridge.hpp"
#include "canvas/scene/bounds_system.hpp"

#include <algorithm>

namespace canvas {

namespace {
RuntimeSceneProjection projectionFromMaterialized(internal::FullMaterializedScene materialized) {
    RuntimeSceneProjection projection;
    projection.generation = materialized.generation;
    projection.records.reserve(materialized.records.size());
    for (auto& record : materialized.records) {
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
    return projection;
}
} // namespace

bool IncrementalRuntimeCoordinator::transactionCheckpoint(
    void* context, std::uint8_t checkpoint) noexcept {
    auto* self = static_cast<IncrementalRuntimeCoordinator*>(context);
    if (self == nullptr) return false;
    return self->checkpointFails(static_cast<RuntimeCheckpoint>(checkpoint));
}

void IncrementalRuntimeCoordinator::publishPending(void* context) noexcept {
    auto* self = static_cast<IncrementalRuntimeCoordinator*>(context);
    if (self != nullptr && self->pendingPublication_.has_value()) {
        self->runtimeScene_.publish(std::move(*self->pendingPublication_));
        self->pendingPublication_.reset();
    }
}

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
        // A dropped ChangeSet is a recovery condition, not an incremental
        // terminal error. Rebuild from the authoritative post-state.
        return recover(compiler, input);
    }
    if (input.changes->beforeGeneration() != runtimeScene_.generation()) {
        SceneCommitInput recoveryInput(input.after_generation, input.post_state);
        auto recoveryMaterialized = internal::materializeFullScene(input.post_state);
        if (!recoveryMaterialized) return foundation::Result<SceneSyncReceipt>::failure(recoveryMaterialized.error());
        auto recoveryPrepared = runtimeScene_.prepare(
            projectionFromMaterialized(std::move(recoveryMaterialized.value())));
        if (!recoveryPrepared) return foundation::Result<SceneSyncReceipt>::failure(recoveryPrepared.error());
        auto recovered = binding_.rebuild(compiler, recoveryInput);
        if (!recovered) return recovered;
        runtimeScene_.publish(std::move(recoveryPrepared.value()));
        return recovered;
    }
    const auto updatePlan = plan(input.post_state, *input.changes);
    if (!updatePlan) {
        if (updatePlan.error().code == foundation::ErrorCode::kInvalidRevision &&
            input.post_state.generation() > runtimeScene_.generation()) {
            SceneCommitInput recoveryInput(input.after_generation, input.post_state);
            auto recoveryMaterialized = internal::materializeFullScene(input.post_state);
            if (!recoveryMaterialized) return foundation::Result<SceneSyncReceipt>::failure(recoveryMaterialized.error());
            auto recoveryPrepared = runtimeScene_.prepare(
                projectionFromMaterialized(std::move(recoveryMaterialized.value())));
            if (!recoveryPrepared) return foundation::Result<SceneSyncReceipt>::failure(recoveryPrepared.error());
            auto recovered = binding_.rebuild(compiler, recoveryInput);
            if (!recovered) return recovered;
            runtimeScene_.publish(std::move(recoveryPrepared.value()));
            return recovered;
        }
        return foundation::Result<SceneSyncReceipt>::failure(updatePlan.error());
    }
    if (checkpointFails(RuntimeCheckpoint::kBeforeRuntimePrepare)) {
        return foundation::Result<SceneSyncReceipt>::failure(
            {foundation::ErrorCode::kParticipantRejected, "RuntimeScene checkpoint failure"});
    }
    auto runtimePrepared = runtimeScene_.prepareIncremental(input.post_state, *input.changes);
    if (!runtimePrepared) {
        return foundation::Result<SceneSyncReceipt>::failure(runtimePrepared.error());
    }
    if (checkpointFails(RuntimeCheckpoint::kAfterRuntimePrepare)) {
        return foundation::Result<SceneSyncReceipt>::failure(
            {foundation::ErrorCode::kParticipantRejected, "RuntimeScene checkpoint failure"});
    }
    // Bounds staging is a real participant operation: derive bounds for each
    // semantically affected object before the Scene transaction prepares
    // record/spatial/invalidation state.  The resulting values are consumed by
    // the compiled SceneDelta; no published participant is touched here.
    for (const auto& change : input.changes->objects()) {
        if (checkpointFails(RuntimeCheckpoint::kBeforeBoundsPrepare)) {
            return foundation::Result<SceneSyncReceipt>::failure(
                {foundation::ErrorCode::kParticipantRejected, "Bounds checkpoint failure"});
        }
        if (const auto* object = input.post_state.find(change.object_id)) {
            const auto bounds = computeBounds(*object);
            if (!bounds.finite) {
                return foundation::Result<SceneSyncReceipt>::failure(
                    {foundation::ErrorCode::kInvalidRecord, "Bounds participant rejected non-finite state"});
            }
        }
        if (checkpointFails(RuntimeCheckpoint::kAfterBoundsPrepare)) {
            return foundation::Result<SceneSyncReceipt>::failure(
                {foundation::ErrorCode::kParticipantRejected, "Bounds checkpoint failure"});
        }
    }
    // The coordinator boundary is the last point at which all participants
    // are still unpublished.  A deterministic failure here must therefore
    // precede SceneBinding's participant prepare/commit transaction.
    pendingPublication_ = std::move(runtimePrepared.value());
    auto incremental = binding_.synchronize(
        compiler, input, &IncrementalRuntimeCoordinator::transactionCheckpoint, this,
        &IncrementalRuntimeCoordinator::publishPending, this);
    if (incremental || incremental.error().code != foundation::ErrorCode::kRequiresFullRebuild) {
        if (!incremental) return incremental;
        return incremental;
    }

    pendingPublication_.reset();

    // An unsafe/unsupported incremental continuation is explicitly recovered
    // through the independent full compiler path. The recovery receipt is
    // distinguishable and never reported as incremental success.
    const SceneCommitInput recoveryInput(input.after_generation, input.post_state);
    auto recoveryMaterialized = internal::materializeFullScene(input.post_state);
    if (!recoveryMaterialized) {
        return foundation::Result<SceneSyncReceipt>::failure(recoveryMaterialized.error());
    }
    auto recoveryPrepared = runtimeScene_.prepare(
        projectionFromMaterialized(std::move(recoveryMaterialized.value())));
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
    auto runtimePrepared = runtimeScene_.prepare(
        projectionFromMaterialized(std::move(materialized.value())));
    if (!runtimePrepared) {
        return foundation::Result<SceneSyncReceipt>::failure(runtimePrepared.error());
    }
    if (checkpointFails(RuntimeCheckpoint::kBeforePublication)) {
        return foundation::Result<SceneSyncReceipt>::failure(
            {foundation::ErrorCode::kParticipantRejected, "Recovery publication checkpoint failure"});
    }
    auto result = binding_.rebuild(compiler, input);
    if (checkpointFails(RuntimeCheckpoint::kAfterPublication)) {
        return foundation::Result<SceneSyncReceipt>::failure(
            {foundation::ErrorCode::kParticipantRejected, "Post-publication checkpoint failure"});
    }
    if (result) runtimeScene_.publish(std::move(runtimePrepared.value()));
    return result;
}

} // namespace canvas
