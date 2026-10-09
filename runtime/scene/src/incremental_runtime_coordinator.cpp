#include "canvas/scene/incremental_runtime_coordinator.hpp"

#include "incremental_runtime_full_materialization_bridge.hpp"
#include "canvas/scene/bounds_system.hpp"
#include "canvas/text/text_layout.hpp"

#include <algorithm>

namespace canvas {

namespace {
// The accepted compiler still owns Scene records. This adapter contributes
// derived text bounds and maps Scene revisions after resource-only updates.
class TextBoundsCompiler final : public ISemanticSceneCompiler {
  public:
    TextBoundsCompiler(const ISemanticSceneCompiler& delegate,const RuntimeSceneProjection& projection,
                       const Scene& scene) : delegate_(delegate), projection_(projection), scene_(scene), before_(scene.revision()) {}
    foundation::Result<CompiledSceneSnapshot> compileFull(const semantic::SemanticReadView& view) const override {
        auto result=delegate_.compileFull(view);
        if(result) {
            result.value().sourceRevision=SceneRevision(std::max(before_.value()+1,view.generation().value()));
            for(auto& record:result.value().records) contribute(record);
        }
        return result;
    }
    foundation::Result<CompiledSceneDelta> compileDelta(const semantic::SemanticReadView& view,
                                                       const semantic::ChangeSet& changes) const override {
        auto result=delegate_.compileDelta(view,changes);
        if(result) {
            result.value().beforeRevision=before_;
            result.value().afterRevision=SceneRevision(std::max(before_.value()+1,view.generation().value()));
            // Resource generations can advance derived bounds without changing
            // canonical state. Only the published Scene owns that before image.
            const auto published=scene_.read();
            for(auto& mutation:result.value().mutations) {
                if(mutation.before && mutation.before->kind==SceneObjectKind::kRichText)
                    if(const auto* before=published.find(mutation.objectId))
                        mutation.before->worldBounds=before->worldBounds;
                if(mutation.after) contribute(*mutation.after);
            }
            if(result.value().hints) {
                result.value().hints->beforeRevision=before_;
                result.value().hints->afterRevision=result.value().afterRevision;
            }
        }
        return result;
    }
  private:
    void contribute(SceneRecord& record) const {
        if(const auto* source=projection_.find(record.objectId);source && source->textLayout)
            record.worldBounds=source->worldBounds;
    }
    const ISemanticSceneCompiler& delegate_; const RuntimeSceneProjection& projection_;
    const Scene& scene_; SceneRevision before_;
};
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
            .textLayout = std::move(record.textLayout),
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

void IncrementalRuntimeCoordinator::observePublication(void* context) noexcept {
    auto* self = static_cast<IncrementalRuntimeCoordinator*>(context);
    if (self != nullptr) {
        ++self->publicationObservations_;
        const auto observed = self->binding_._scene.read();
        const auto sceneRevision = self->binding_._scene.revision();
        const auto sceneGeneration = self->binding_._scene.semanticGeneration();
        const auto runtimeGeneration = self->runtimeScene_.generation();
        if (sceneRevision != self->publicationGate_.previousRevision ||
            sceneGeneration != self->publicationGate_.previousGeneration ||
            runtimeGeneration != self->publicationGate_.previousGeneration ||
            observed.revision() != self->publicationGate_.previousRevision) {
            self->publicationObservationCoherent_ = false;
        }
        const auto hit = self->binding_._scene.hitTest(
            HitTestRequest{WorldPoint{0.0F, 0.0F}, 0.0F, HitTestFilter{}, 1U});
        if (hit || hit.error().code != foundation::ErrorCode::kParticipantRejected) {
            self->publicationObservationCoherent_ = false;
        }
        const auto query = self->binding_._scene.query(SceneQuery{WorldRect{-1.0F, -1.0F, 1.0F, 1.0F}});
        if (!query) {
            self->publicationObservationCoherent_ = false;
        } else {
            const auto draw = self->binding_._scene.buildDrawList(query.value());
            if (draw || draw.error().code != foundation::ErrorCode::kParticipantRejected) {
                self->publicationObservationCoherent_ = false;
            }
        }
        if (self->testPublicationObserver_ != nullptr) {
            self->testPublicationObserver_(self->testPublicationObserverContext_);
        }
    }
}

void IncrementalRuntimeCoordinator::publishPending(void* context) noexcept {
    auto* self = static_cast<IncrementalRuntimeCoordinator*>(context);
    if (self != nullptr && self->pendingPublication_.has_value()) {
        self->runtimeScene_.publish(std::move(*self->pendingPublication_));
        self->pendingPublication_.reset();
        self->binding_._scene.stageSemanticGeneration(self->pendingGeneration_);
        self->binding_._scene.publishStagedSnapshot();
        if (self->publicationGate_.observation != nullptr) {
            self->publicationGate_.observation(self->publicationGate_.observationContext);
        }
        if (self->testPublicationObserver_ != nullptr) {
            self->testPublicationObserver_(self->testPublicationObserverContext_);
        }
        self->publicationGate_.transactionActive = false;
        self->publicationGate_.observation = nullptr;
        self->publicationGate_.observationContext = nullptr;
    }
}

void IncrementalRuntimeCoordinator::abortPublication() noexcept {
    pendingPublication_.reset();
    binding_._scene.clearPendingPublication();
    publicationGate_.transactionActive = false;
    publicationGate_.observation = nullptr;
    publicationGate_.observationContext = nullptr;
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
        return recover(compiler, recoveryInput);
    }
    const auto updatePlan = plan(input.post_state, *input.changes);
    if (!updatePlan) {
        if (updatePlan.error().code == foundation::ErrorCode::kInvalidRevision &&
            input.post_state.generation() > runtimeScene_.generation()) {
            SceneCommitInput recoveryInput(input.after_generation, input.post_state);
            return recover(compiler, recoveryInput);
        }
        return foundation::Result<SceneSyncReceipt>::failure(updatePlan.error());
    }
    publicationGate_.previousGeneration = runtimeScene_.generation();
    publicationGate_.previousRevision = binding_._scene.revision();
    publicationGate_.generation = input.after_generation;
    publicationGate_.revision = SceneRevision(input.after_generation.value());
    publicationGate_.transactionActive = true;
    publicationGate_.observation = &IncrementalRuntimeCoordinator::observePublication;
    publicationGate_.observationContext = this;
    runtimeScene_._stableProjection = runtimeScene_._projection;
    binding_._scene._stablePublishedRecords = binding_._scene._publishedRecords;
    binding_._scene._stablePublishedBounds = binding_._scene._publishedBounds;
    binding_._scene._stableInvalidationGeneration = binding_._scene._invalidationGeneration;
    binding_._scene._stablePublishedInvalidation = binding_._scene._publishedInvalidation;
    if (checkpointFails(RuntimeCheckpoint::kBeforeRuntimePrepare)) {
        abortPublication();
        return foundation::Result<SceneSyncReceipt>::failure(
            {foundation::ErrorCode::kParticipantRejected, "RuntimeScene checkpoint failure"});
    }
    auto runtimePrepared = runtimeScene_.prepareIncremental(input.post_state, *input.changes);
    if (!runtimePrepared) {
        abortPublication();
        return foundation::Result<SceneSyncReceipt>::failure(runtimePrepared.error());
    }
    if (checkpointFails(RuntimeCheckpoint::kAfterRuntimePrepare)) {
        abortPublication();
        return foundation::Result<SceneSyncReceipt>::failure(
            {foundation::ErrorCode::kParticipantRejected, "RuntimeScene checkpoint failure"});
    }
    // Bounds staging is a real participant operation: derive bounds for each
    // semantically affected object before the Scene transaction prepares
    // record/spatial/invalidation state.  The resulting values are consumed by
    // the compiled SceneDelta; no published participant is touched here.
    for (const auto& change : input.changes->objects()) {
        if (checkpointFails(RuntimeCheckpoint::kBeforeBoundsPrepare)) {
            abortPublication();
            return foundation::Result<SceneSyncReceipt>::failure(
                {foundation::ErrorCode::kParticipantRejected, "Bounds checkpoint failure"});
        }
        if (const auto* object = input.post_state.find(change.object_id)) {
            const auto* derived=runtimePrepared.value().projection.find(object->id);
            const auto bounds = computeBounds(*object,derived ? derived->textLayout.get() : nullptr);
            if (!bounds.finite) {
                abortPublication();
                return foundation::Result<SceneSyncReceipt>::failure(
                    {foundation::ErrorCode::kInvalidRecord, "Bounds participant rejected non-finite state"});
            }
            binding_._scene.stageBounds(bounds.world, SceneRevision(input.after_generation.value()));
        }
        if (checkpointFails(RuntimeCheckpoint::kAfterBoundsPrepare)) {
            abortPublication();
            return foundation::Result<SceneSyncReceipt>::failure(
                {foundation::ErrorCode::kParticipantRejected, "Bounds checkpoint failure"});
        }
    }
    // The coordinator boundary is the last point at which all participants
    // are still unpublished.  A deterministic failure here must therefore
    // precede SceneBinding's participant prepare/commit transaction.
    pendingPublication_ = std::move(runtimePrepared.value());
    pendingGeneration_ = input.after_generation;
    pendingRevision_ = SceneRevision(input.after_generation.value());
    const TextBoundsCompiler textCompiler(compiler,pendingPublication_->projection,binding_._scene);
    auto incremental = binding_.synchronize(
        textLayoutService_ ? static_cast<const ISemanticSceneCompiler&>(textCompiler) : compiler,
        input, &IncrementalRuntimeCoordinator::transactionCheckpoint, this,
        &IncrementalRuntimeCoordinator::publishPending, this);
    if (incremental) {
        return incremental;
    }
    abortPublication();
    if (incremental.error().code != foundation::ErrorCode::kRequiresFullRebuild) {
        return incremental;
    }

    // An unsafe/unsupported incremental continuation is explicitly recovered
    // through the independent full compiler path. The recovery receipt is
    // distinguishable and never reported as incremental success.
    const SceneCommitInput recoveryInput(input.after_generation, input.post_state);
    auto recovered = recover(compiler, recoveryInput);
    if (!recovered) return recovered;
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
    auto materialized = internal::materializeFullScene(input.post_state,textLayoutService_);
    if (!materialized) {
        return foundation::Result<SceneSyncReceipt>::failure(materialized.error());
    }
    auto runtimePrepared = runtimeScene_.prepare(
        projectionFromMaterialized(std::move(materialized.value())));
    if (!runtimePrepared) {
        return foundation::Result<SceneSyncReceipt>::failure(runtimePrepared.error());
    }
    publicationGate_.previousGeneration = runtimeScene_.generation();
    publicationGate_.previousRevision = binding_._scene.revision();
    publicationGate_.generation = input.after_generation;
    publicationGate_.revision = SceneRevision(input.after_generation.value());
    publicationGate_.transactionActive = true;
    publicationGate_.observation = &IncrementalRuntimeCoordinator::observePublication;
    publicationGate_.observationContext = this;
    runtimeScene_._stableProjection = runtimeScene_._projection;
    binding_._scene._stablePublishedRecords = binding_._scene._publishedRecords;
    binding_._scene._stablePublishedBounds = binding_._scene._publishedBounds;
    binding_._scene._stableInvalidationGeneration = binding_._scene._invalidationGeneration;
    binding_._scene._stablePublishedInvalidation = binding_._scene._publishedInvalidation;
    if (checkpointFails(RuntimeCheckpoint::kBeforePublication)) {
        abortPublication();
        return foundation::Result<SceneSyncReceipt>::failure(
            {foundation::ErrorCode::kParticipantRejected, "Recovery publication checkpoint failure"});
    }
    const TextBoundsCompiler textCompiler(compiler,runtimePrepared.value().projection,binding_._scene);
    auto result = binding_.rebuild(textLayoutService_ ? static_cast<const ISemanticSceneCompiler&>(textCompiler) : compiler, input);
    if (!result) {
        abortPublication();
        return result;
    }
    binding_._scene.stageSemanticGeneration(input.after_generation);
    binding_._scene.publishStagedSnapshot();
    runtimeScene_.publish(std::move(runtimePrepared.value()));
    if (publicationGate_.observation != nullptr) {
        publicationGate_.observation(publicationGate_.observationContext);
    }
    if (testPublicationObserver_ != nullptr) {
        testPublicationObserver_(testPublicationObserverContext_);
    }
    publicationGate_.transactionActive = false;
    publicationGate_.observation = nullptr;
    publicationGate_.observationContext = nullptr;
    return result;
}

foundation::Result<SceneSyncReceipt> IncrementalRuntimeCoordinator::refreshTextResources() {
    if(!textLayoutService_) return foundation::Result<SceneSyncReceipt>::failure(
        {foundation::ErrorCode::kInvalidArgument,"No application text layout service is bound"});
    auto next=runtimeScene_._projection;
    CompiledSceneDelta delta{binding_._scene.revision(),SceneRevision(binding_._scene.revision().value()+1),{},std::nullopt};
    const auto published=binding_._scene.read();
    for(auto& record:next.records) {
        if(record.kind!=semantic::ObjectKind::kRichText) continue;
        auto layout=textLayoutService_->resolve(record.objectId,std::get<semantic::RichTextContent>(record.content));
        if(record.textLayout && record.textLayout->digest==layout->digest) continue;
        semantic::ObjectRecord source{}; source.id=record.objectId; source.kind=record.kind;
        source.transform=record.transform; source.properties=record.properties; source.content=record.content;
        const auto bounds=computeBounds(source,layout.get());
        if(!bounds.finite) return foundation::Result<SceneSyncReceipt>::failure(
            {foundation::ErrorCode::kInvalidRecord,"Resource-derived text bounds are non-finite"});
        record.textLayout=std::move(layout); record.geometryBounds=bounds.geometry;
        record.visualBounds=bounds.visual; record.worldBounds=bounds.world;
        record.referenceGeometryDigest=record.textLayout->digest;
        const auto* before=published.find(record.objectId);
        if(!before) return foundation::Result<SceneSyncReceipt>::failure(
            {foundation::ErrorCode::kMissingObject,"Text contribution missing from published Scene"});
        auto after=*before; after.worldBounds=bounds.world;
        delta.mutations.push_back({SceneMutationKind::kUpdate,record.objectId,*before,after});
    }
    if(delta.mutations.empty()) return foundation::Result<SceneSyncReceipt>::success(
        {published.revision(),runtimeScene_.generation(),SceneSyncDisposition::kAppliedIncremental,
         SceneApplyReceipt{published.revision(),published.revision(),0,0,0,{}},std::nullopt});
    publicationGate_.previousGeneration=runtimeScene_.generation();
    publicationGate_.previousRevision=published.revision();
    publicationGate_.generation=next.generation; publicationGate_.revision=delta.afterRevision;
    publicationGate_.transactionActive=true;
    publicationGate_.observation=&IncrementalRuntimeCoordinator::observePublication;
    publicationGate_.observationContext=this;
    runtimeScene_._stableProjection=runtimeScene_._projection;
    binding_._scene._stablePublishedRecords=binding_._scene._publishedRecords;
    binding_._scene._stablePublishedBounds=binding_._scene._publishedBounds;
    binding_._scene._stableInvalidationGeneration=binding_._scene._invalidationGeneration;
    binding_._scene._stablePublishedInvalidation=binding_._scene._publishedInvalidation;
    pendingGeneration_=next.generation; pendingRevision_=delta.afterRevision;
    pendingPublication_=RuntimeScene::PreparedPublication{std::move(next)};
    auto applied=binding_._scene.applyPreparedDelta(std::move(delta),
        &IncrementalRuntimeCoordinator::transactionCheckpoint,this,
        &IncrementalRuntimeCoordinator::publishPending,this);
    if(!applied) { abortPublication(); return foundation::Result<SceneSyncReceipt>::failure(applied.error()); }
    return foundation::Result<SceneSyncReceipt>::success({applied.value().afterRevision,pendingGeneration_,
        SceneSyncDisposition::kAppliedIncremental,std::move(applied.value()),std::nullopt});
}

} // namespace canvas
