#include "canvas/scene/scene.hpp"
#include "canvas/scene/scene_delta.hpp"
#include "canvas/scene/spatial_delta.hpp"
#include "canvas/scene/scene_impact.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <limits>
#include <new>
#include <unordered_set>
#include <utility>

namespace canvas {
namespace {

foundation::Error makeError(foundation::ErrorCode code, const char* message) {
    return foundation::Error{code, message};
}

bool isSupportedRuntimeKind(semantic::ObjectKind kind) noexcept {
    switch (kind) {
    case semantic::ObjectKind::kShape:
    case semantic::ObjectKind::kImage:
    case semantic::ObjectKind::kVectorPath:
    case semantic::ObjectKind::kRichText:
    case semantic::ObjectKind::kVectorStroke:
    case semantic::ObjectKind::kDabStroke:
    case semantic::ObjectKind::kConnector:
    case semantic::ObjectKind::kSticky:
    case semantic::ObjectKind::kGroup:
        return true;
    }
    return false;
}

std::vector<SpatialRecord> makeSpatialRecords(std::span<const SceneRecord> records) {
    std::vector<SpatialRecord> spatialRecords;
    spatialRecords.reserve(records.size());
    for (const SceneRecord& record : records) {
        spatialRecords.push_back(SpatialRecord{record.objectId, record.worldBounds});
    }
    return spatialRecords;
}

DamageSet damageForDelta(const CompiledSceneDelta& delta) {
    DamageSet damage{
        .afterExclusive = delta.beforeRevision,
        .throughInclusive = delta.afterRevision,
        .fullScene = false,
        .rects = {},
    };
    damage.rects.reserve(delta.mutations.size());
    for (const SceneMutation& mutation : delta.mutations) {
        WorldRect dirty =
            mutation.before ? mutation.before->worldBounds : mutation.after->worldBounds;
        if (mutation.before && mutation.after) {
            dirty =
                foundation::unionRects(mutation.before->worldBounds, mutation.after->worldBounds);
        }
        damage.rects.push_back(DamageRect{dirty, DamageReason::kContent});
    }
    return damage;
}

bool isVisible(const SceneRecord& record) {
    return (static_cast<std::uint32_t>(record.flags) &
            static_cast<std::uint32_t>(SceneRecordFlags::kVisible)) != 0;
}

bool hasFlag(SceneRecordFlags value, SceneRecordFlags flag) {
    return (static_cast<std::uint32_t>(value) & static_cast<std::uint32_t>(flag)) != 0U;
}

std::uint32_t kindBit(SceneObjectKind kind) {
    switch (kind) {
    case SceneObjectKind::kShape:
        return static_cast<std::uint32_t>(HitTestKindMask::kShape);
    case SceneObjectKind::kImage:
        return static_cast<std::uint32_t>(HitTestKindMask::kImage);
    case SceneObjectKind::kVectorPath:
        return static_cast<std::uint32_t>(HitTestKindMask::kVectorPath);
    case SceneObjectKind::kRichText:
        return static_cast<std::uint32_t>(HitTestKindMask::kRichText);
    case SceneObjectKind::kVectorStroke:
        return static_cast<std::uint32_t>(HitTestKindMask::kVectorStroke);
    case SceneObjectKind::kDabStroke:
        return static_cast<std::uint32_t>(HitTestKindMask::kDabStroke);
    }
    return 0U;
}

bool hasKnownHitKinds(HitTestKindMask kinds) {
    return (static_cast<std::uint32_t>(kinds) &
            ~static_cast<std::uint32_t>(HitTestKindMask::kAll)) == 0U;
}

} // namespace

foundation::Result<RuntimeSceneProjection> RuntimeScene::replace(
    const semantic::SemanticReadView& post_state) {
    auto prepared = prepare(post_state);
    if (!prepared) return foundation::Result<RuntimeSceneProjection>::failure(prepared.error());
    RuntimeSceneProjection result = prepared.value().projection;
    publish(std::move(prepared.value()));
    return foundation::Result<RuntimeSceneProjection>::success(std::move(result));
}

foundation::Result<RuntimeScene::PreparedPublication> RuntimeScene::prepare(
    const semantic::SemanticReadView& post_state) const {
    try {
        const std::vector<semantic::ObjectRecord> source = post_state.allObjects();
        for (const semantic::ObjectRecord& record : source) {
            if (record.id.isZero() || !isSupportedRuntimeKind(record.kind)) {
                return foundation::Result<RuntimeScene::PreparedPublication>::failure(
                    makeError(foundation::ErrorCode::kInvalidRecord,
                              "RuntimeScene contains an unknown or invalid object kind"));
            }
        }
        return foundation::Result<PreparedPublication>::success(
            PreparedPublication{projectRuntimeScene(source, post_state.generation())});
    } catch (const std::bad_alloc&) {
        return foundation::Result<PreparedPublication>::failure(
            makeError(foundation::ErrorCode::kOutOfMemory,
                      "Unable to prepare renderer-neutral RuntimeScene projection"));
    }
}

foundation::Result<RuntimeScene::PreparedPublication> RuntimeScene::prepare(
    RuntimeSceneProjection projection) const {
    return foundation::Result<PreparedPublication>::success(
        PreparedPublication{std::move(projection)});
}

foundation::Result<RuntimeScene::PreparedPublication> RuntimeScene::prepareIncremental(
    const semantic::SemanticReadView& post_state,
    const semantic::ChangeSet& changes) const {
    if (changes.beforeGeneration() != _projection.generation ||
        changes.afterGeneration() != post_state.generation()) {
        return foundation::Result<PreparedPublication>::failure(
            makeError(foundation::ErrorCode::kInvalidRevision,
                      "RuntimeScene incremental generation does not match published state"));
    }
    try {
        RuntimeSceneProjection next = _projection;
        next.generation = post_state.generation();
        for (const auto& change : changes.objects()) {
            const auto* current = post_state.find(change.object_id);
            const auto impact = scene::classifyImpact(change, current);
            (void)impact;
            const auto existing = std::find_if(
                next.records.begin(), next.records.end(), [&](const RuntimeSceneRecord& record) {
                    return record.objectId == change.object_id;
                });
            const bool deleted = (static_cast<unsigned char>(change.flags) &
                                  static_cast<unsigned char>(semantic::SemanticChangeFlags::kDeleted)) != 0;
            if (deleted || current == nullptr) {
                if (existing != next.records.end()) next.records.erase(existing);
                continue;
            }
            auto projected = projectRuntimeScene(std::span<const semantic::ObjectRecord>(current, 1),
                                                 post_state.generation());
            if (existing == next.records.end()) {
                next.records.push_back(std::move(projected.records.front()));
            } else {
                *existing = std::move(projected.records.front());
            }
        }
        std::sort(next.records.begin(), next.records.end(),
                  [](const RuntimeSceneRecord& left, const RuntimeSceneRecord& right) {
                      if (left.placement.order_key != right.placement.order_key) {
                          return left.placement.order_key < right.placement.order_key;
                      }
                      return left.objectId < right.objectId;
                  });
        return foundation::Result<PreparedPublication>::success(
            PreparedPublication{std::move(next)});
    } catch (const std::bad_alloc&) {
        return foundation::Result<PreparedPublication>::failure(
            makeError(foundation::ErrorCode::kOutOfMemory,
                      "Unable to prepare incremental RuntimeScene projection"));
    }
}

void RuntimeScene::publish(PreparedPublication publication) noexcept {
    _stableProjection = std::move(_projection);
    _projection = std::move(publication.projection);
}

foundation::Result<RuntimeSceneProjection> RuntimeScene::apply(
    const semantic::SemanticReadView& post_state) {
    if (post_state.generation() <= _projection.generation) {
        return foundation::Result<RuntimeSceneProjection>::failure(
            makeError(foundation::ErrorCode::kInvalidRevision,
                      "RuntimeScene generation must advance on apply"));
    }
    return replace(post_state);
}

const SceneRecord* SceneReadView::find(ObjectId objectId) const {
    const auto found =
        std::find_if(_records.begin(), _records.end(), [objectId](const SceneRecord& record) {
            return record.objectId == objectId;
        });
    return found == _records.end() ? nullptr : &*found;
}

Scene::Scene(std::unique_ptr<IRenderScene> renderScene, std::unique_ptr<ISpatialIndex> spatialIndex)
    : _renderScene(std::move(renderScene)), _spatialIndex(std::move(spatialIndex)) {
    assert(_renderScene != nullptr);
    assert(_spatialIndex != nullptr);
}

bool Scene::stagePublicationSnapshot(std::span<const SceneRecord> records) {
    try {
        _stagedPublishedRecords.assign(records.begin(), records.end());
        return true;
    } catch (const std::bad_alloc&) {
        return false;
    }
}

void Scene::publishStagedSnapshot() noexcept {
    _stablePublishedRecords = std::move(_publishedRecords);
    _stablePublishedBounds = _publishedBounds;
    _stableInvalidationGeneration = _invalidationGeneration;
    _stablePublishedInvalidation = _publishedInvalidation;
    _publishedRecords.swap(_stagedPublishedRecords);
    _publishedSnapshotValid = true;
    _publishedBounds = _pendingBounds;
    _invalidationGeneration = _pendingInvalidationGeneration;
    _publishedSemanticGeneration = _pendingSemanticGeneration;
    _semanticGeneration = _pendingSemanticGeneration;
    _publishedInvalidation = std::move(_pendingInvalidation);
    _pendingBoundsStaged = false;
}

foundation::Result<SceneApplyReceipt> Scene::replace(CompiledSceneSnapshot snapshot) {
    if (snapshot.sourceRevision.isZero() || snapshot.sourceRevision <= _revision) {
        return foundation::Result<SceneApplyReceipt>::failure(makeError(
            foundation::ErrorCode::kInvalidRevision, "Snapshot revision must advance Scene"));
    }

    auto recordsResult = _records.prepareReplace(snapshot.records);
    if (!recordsResult) {
        return foundation::Result<SceneApplyReceipt>::failure(recordsResult.error());
    }
    SceneRecordStore::PreparedUpdate preparedRecords = std::move(recordsResult.value());

    try {
        std::vector<SpatialRecord> spatialRecords = makeSpatialRecords(preparedRecords.records());
        auto renderResult =
            _renderScene->prepareReplace(preparedRecords.records(), snapshot.sourceRevision);
        if (!renderResult) {
            return foundation::Result<SceneApplyReceipt>::failure(renderResult.error());
        }
        if (!renderResult.value()) {
            return foundation::Result<SceneApplyReceipt>::failure(
                makeError(foundation::ErrorCode::kParticipantRejected,
                          "Render participant returned an empty prepared update"));
        }

        auto spatialResult = _spatialIndex->prepareReplace(spatialRecords, snapshot.sourceRevision);
        if (!spatialResult) {
            return foundation::Result<SceneApplyReceipt>::failure(spatialResult.error());
        }
        if (!spatialResult.value()) {
            return foundation::Result<SceneApplyReceipt>::failure(
                makeError(foundation::ErrorCode::kParticipantRejected,
                          "Spatial participant returned an empty prepared update"));
        }

        const WorldRect oldContentBounds = _records.contentBounds();
        const WorldRect newContentBounds = preparedRecords.contentBounds();
        auto damageResult = _damageTracker.prepareReplace(
            _revision, snapshot.sourceRevision, oldContentBounds, newContentBounds);
        if (!damageResult) {
            return foundation::Result<SceneApplyReceipt>::failure(damageResult.error());
        }

        SceneApplyReceipt receipt{
            .beforeRevision = _revision,
            .afterRevision = snapshot.sourceRevision,
            .recordsTouched = preparedRecords.records().size(),
            .renderNodesTouched = preparedRecords.records().size(),
            .spatialRecordsTouched = preparedRecords.records().size(),
            .damage =
                DamageSet{
                    .afterExclusive = _revision,
                    .throughInclusive = snapshot.sourceRevision,
                    .fullScene = true,
                    .rects = {DamageRect{
                        .worldRect = foundation::unionRects(oldContentBounds, newContentBounds),
                        .reasons = DamageReason::kFullRebuild,
                    }},
                },
        };

        if (!stagePublicationSnapshot(preparedRecords.records())) {
            return foundation::Result<SceneApplyReceipt>::failure(makeError(
                foundation::ErrorCode::kOutOfMemory, "Unable to stage published Scene snapshot"));
        }

        _commitDiagnostics = SceneCommitDiagnostics{
            .transactionCount = _commitDiagnostics.transactionCount + 1,
        };
        std::uint8_t stage = 0;
        _records.commit(std::move(preparedRecords));
        _commitDiagnostics.recordStoreStage = ++stage;
        if (_publicationGate != nullptr && _publicationGate->observation != nullptr) {
            _publicationGate->observation(_publicationGate->observationContext);
        }
        _renderScene->commit(std::move(renderResult.value()));
        _commitDiagnostics.renderSceneStage = ++stage;
        if (_publicationGate != nullptr && _publicationGate->observation != nullptr) {
            _publicationGate->observation(_publicationGate->observationContext);
        }
        _spatialIndex->commit(std::move(spatialResult.value()));
        _commitDiagnostics.spatialIndexStage = ++stage;
        if (_publicationGate != nullptr && _publicationGate->observation != nullptr) {
            _publicationGate->observation(_publicationGate->observationContext);
        }
        _damageTracker.commit(std::move(damageResult.value()));
        _commitDiagnostics.damageStage = ++stage;
        if (_publicationGate != nullptr && _publicationGate->observation != nullptr) {
            _publicationGate->observation(_publicationGate->observationContext);
        }
        _revision = snapshot.sourceRevision;
        _pendingBounds = newContentBounds;
        _pendingBoundsStaged = true;
        _pendingInvalidationGeneration = snapshot.sourceRevision;
        _pendingSemanticGeneration = semantic::SemanticGeneration(snapshot.sourceRevision.value());
        _pendingInvalidation = SceneInvalidationOutput{snapshot.sourceRevision, receipt.damage.rects, true};
        if (_publicationGate == nullptr || !_publicationGate->transactionActive) {
            publishStagedSnapshot();
        }
        _commitDiagnostics.revisionStage = ++stage;
        return foundation::Result<SceneApplyReceipt>::success(std::move(receipt));
    } catch (const std::bad_alloc&) {
        return foundation::Result<SceneApplyReceipt>::failure(
            makeError(foundation::ErrorCode::kOutOfMemory, "Unable to prepare Scene replacement"));
    }
}

foundation::Result<SceneApplyReceipt> Scene::replace(const SceneCommitInput& input,
                                                    CompiledSceneSnapshot snapshot) {
    const bool initialGenerationBaseline =
        _revision.isZero() && _semanticGeneration == semantic::SemanticGeneration(0) &&
        input.before_generation == semantic::SemanticGeneration(0) &&
        input.after_generation == semantic::SemanticGeneration(0);
    if (input.post_state.generation() != input.after_generation ||
        input.before_generation != input.after_generation ||
        (!initialGenerationBaseline && input.after_generation <= _semanticGeneration)) {
        return foundation::Result<SceneApplyReceipt>::failure(makeError(
            foundation::ErrorCode::kInvalidRevision,
            "SceneCommitInput generation does not match full replacement"));
    }
    auto result = replace(std::move(snapshot));
    if (result) {
        _semanticGeneration = input.after_generation;
    }
    return result;
}

foundation::Result<SceneApplyReceipt> Scene::apply(CompiledSceneDelta delta) {
    return applyPreparedDelta(std::move(delta), nullptr, nullptr, nullptr, nullptr);
}

foundation::Result<SceneApplyReceipt> Scene::applyPreparedDelta(
    CompiledSceneDelta delta,
    TransactionCheckpointFn checkpoint,
    void* checkpointContext,
    PublicationFn publish,
    void* publishContext) {
    if (delta.beforeRevision != _revision || delta.afterRevision <= delta.beforeRevision) {
        return foundation::Result<SceneApplyReceipt>::failure(makeError(
            foundation::ErrorCode::kInvalidRevision, "Delta does not advance the current Scene"));
    }

    auto recordsResult = _records.prepareApply(delta.mutations);
    if (!recordsResult) {
        return foundation::Result<SceneApplyReceipt>::failure(recordsResult.error());
    }
    SceneRecordStore::PreparedUpdate preparedRecords = std::move(recordsResult.value());

    try {
        std::vector<SceneRecord> stagedRecords;
        auto currentRecords = _records.materializeSnapshot();
        if (!currentRecords) {
            return foundation::Result<SceneApplyReceipt>::failure(currentRecords.error());
        }
        stagedRecords = std::move(currentRecords.value());
        for (const auto& mutation : delta.mutations) {
            auto it = std::find_if(stagedRecords.begin(), stagedRecords.end(),
                                   [&](const SceneRecord& record) {
                                       return record.objectId == mutation.objectId;
                                   });
            if (mutation.kind == SceneMutationKind::kInsert && mutation.after) {
                stagedRecords.push_back(*mutation.after);
            } else if (mutation.kind == SceneMutationKind::kUpdate && mutation.after &&
                       it != stagedRecords.end()) {
                *it = *mutation.after;
            } else if (mutation.kind == SceneMutationKind::kRemove && it != stagedRecords.end()) {
                stagedRecords.erase(it);
            }
        }
        std::sort(stagedRecords.begin(), stagedRecords.end(), [](const SceneRecord& a,
                                                                 const SceneRecord& b) {
            return a.orderKey < b.orderKey ||
                   (a.orderKey == b.orderKey && a.objectId < b.objectId);
        });
        if (!stagePublicationSnapshot(stagedRecords)) {
            return foundation::Result<SceneApplyReceipt>::failure(makeError(
                foundation::ErrorCode::kOutOfMemory, "Unable to stage published Scene snapshot"));
        }
        const WorldRect newContentBounds = preparedRecords.contentBounds();
        const SceneDelta runtimeDelta = makeSceneDelta(delta);
        if (checkpoint != nullptr && checkpoint(checkpointContext, 4U)) {
            return foundation::Result<SceneApplyReceipt>::failure(makeError(
                foundation::ErrorCode::kParticipantRejected, "Spatial checkpoint failure"));
        }
        auto renderResult =
            _renderScene->prepareDelta(runtimeDelta, delta.beforeRevision, delta.afterRevision);
        if (!renderResult) {
            return foundation::Result<SceneApplyReceipt>::failure(renderResult.error());
        }
        if (!renderResult.value()) {
            return foundation::Result<SceneApplyReceipt>::failure(
                makeError(foundation::ErrorCode::kParticipantRejected,
                          "Render participant returned an empty prepared update"));
        }

        const SpatialDelta spatialDelta = makeSpatialDelta(runtimeDelta);
        auto spatialResult = _spatialIndex->prepareSpatialDelta(
            spatialDelta, delta.beforeRevision, delta.afterRevision);
        if (!spatialResult) {
            return foundation::Result<SceneApplyReceipt>::failure(spatialResult.error());
        }
        if (!spatialResult.value()) {
            return foundation::Result<SceneApplyReceipt>::failure(
                makeError(foundation::ErrorCode::kParticipantRejected,
                          "Spatial participant returned an empty prepared update"));
        }
        if (checkpoint != nullptr && checkpoint(checkpointContext, 5U)) {
            return foundation::Result<SceneApplyReceipt>::failure(makeError(
                foundation::ErrorCode::kParticipantRejected, "Spatial checkpoint failure"));
        }

        const DamageSet stagedDamage = damageForDelta(delta);
        if (checkpoint != nullptr && checkpoint(checkpointContext, 6U)) {
            return foundation::Result<SceneApplyReceipt>::failure(makeError(
                foundation::ErrorCode::kParticipantRejected, "Invalidation checkpoint failure"));
        }
        _pendingInvalidation = SceneInvalidationOutput{
            delta.afterRevision, stagedDamage.rects, false};
        if (checkpoint != nullptr && checkpoint(checkpointContext, 7U)) {
            return foundation::Result<SceneApplyReceipt>::failure(makeError(
                foundation::ErrorCode::kParticipantRejected, "Invalidation checkpoint failure"));
        }
        auto damageResult = _damageTracker.prepareApply(delta);
        if (!damageResult) {
            return foundation::Result<SceneApplyReceipt>::failure(damageResult.error());
        }
        SceneApplyReceipt receipt{
            .beforeRevision = delta.beforeRevision,
            .afterRevision = delta.afterRevision,
            .recordsTouched = delta.mutations.size(),
            .renderNodesTouched = delta.mutations.size(),
            .spatialRecordsTouched = delta.mutations.size(),
            .damage = damageForDelta(delta),
        };

        if (checkpoint != nullptr && checkpoint(checkpointContext, 8U)) {
            return foundation::Result<SceneApplyReceipt>::failure(makeError(
                foundation::ErrorCode::kParticipantRejected, "Publication checkpoint failure"));
        }

        _commitDiagnostics = SceneCommitDiagnostics{
            .transactionCount = _commitDiagnostics.transactionCount + 1,
        };
        std::uint8_t stage = 0;
        _records.commit(std::move(preparedRecords));
        _commitDiagnostics.recordStoreStage = ++stage;
        if (_publicationGate != nullptr && _publicationGate->observation != nullptr) {
            _publicationGate->observation(_publicationGate->observationContext);
        }
        _renderScene->commit(std::move(renderResult.value()));
        _commitDiagnostics.renderSceneStage = ++stage;
        if (_publicationGate != nullptr && _publicationGate->observation != nullptr) {
            _publicationGate->observation(_publicationGate->observationContext);
        }
        _spatialIndex->commit(std::move(spatialResult.value()));
        _commitDiagnostics.spatialIndexStage = ++stage;
        if (_publicationGate != nullptr && _publicationGate->observation != nullptr) {
            _publicationGate->observation(_publicationGate->observationContext);
        }
        _damageTracker.commit(std::move(damageResult.value()));
        _commitDiagnostics.damageStage = ++stage;
        if (_publicationGate != nullptr && _publicationGate->observation != nullptr) {
            _publicationGate->observation(_publicationGate->observationContext);
        }
        _revision = delta.afterRevision;
        if (!_pendingBoundsStaged) {
            _pendingBounds = newContentBounds;
        }
        _pendingInvalidationGeneration = delta.afterRevision;
        _pendingSemanticGeneration = semantic::SemanticGeneration(delta.afterRevision.value());
        // The generation-bound invalidation payload was staged inside the
        // canonical invalidation checkpoint pair above.
        _commitDiagnostics.revisionStage = ++stage;
        // All participant commits have completed.  The coordinator closes the
        // shared publication gate only after this point, so observers cannot
        // observe a mixed generation while compatibility participants commit.
        if (publish != nullptr) {
            publish(publishContext);
        } else {
            publishStagedSnapshot();
        }
        return foundation::Result<SceneApplyReceipt>::success(std::move(receipt));
    } catch (const std::bad_alloc&) {
        return foundation::Result<SceneApplyReceipt>::failure(
            makeError(foundation::ErrorCode::kOutOfMemory, "Unable to prepare Scene delta"));
    }
}

foundation::Result<SceneApplyReceipt> Scene::apply(const SceneCommitInput& input,
                                                  CompiledSceneDelta delta) {
    if (input.changes == nullptr || input.post_state.generation() != input.after_generation ||
        input.changes->beforeGeneration() != input.before_generation ||
        input.changes->afterGeneration() != input.after_generation ||
        input.before_generation != _semanticGeneration ||
        input.after_generation <= input.before_generation) {
        return foundation::Result<SceneApplyReceipt>::failure(makeError(
            foundation::ErrorCode::kInvalidRevision,
            "SceneCommitInput generation transition is invalid"));
    }
    auto result = apply(std::move(delta));
    if (result) {
        _semanticGeneration = input.after_generation;
    }
    return result;
}

foundation::Result<SceneQueryResult> Scene::query(const SceneQuery& request) const {
    if (!request.worldRect.isFiniteAndOrdered()) {
        return foundation::Result<SceneQueryResult>::failure(
            makeError(foundation::ErrorCode::kInvalidArgument, "Scene query bounds are invalid"));
    }
    if (_publicationGate != nullptr && _publicationGate->transactionActive &&
        _publishedSnapshotValid) {
        try {
            SceneQueryResult result{
                .revision = _publicationGate->previousRevision,
                .backToFront = {},
                .diagnostics = {},
            };
            for (const auto& record : _stablePublishedRecords) {
                if (isVisible(record) && record.worldBounds.intersects(request.worldRect)) {
                    result.backToFront.push_back(record.objectId);
                }
            }
            result.diagnostics.visibleRecords = result.backToFront.size();
            return foundation::Result<SceneQueryResult>::success(std::move(result));
        } catch (const std::bad_alloc&) {
            return foundation::Result<SceneQueryResult>::failure(
                makeError(foundation::ErrorCode::kOutOfMemory, "Scene query could not allocate"));
        }
    }
    auto spatialResult = _spatialIndex->query(request.worldRect);
    if (!spatialResult) {
        return foundation::Result<SceneQueryResult>::failure(spatialResult.error());
    }
    try {
        SceneQueryResult result{
            .revision = _revision,
            .backToFront = {},
            .diagnostics =
                SceneQueryDiagnostics{
                    .candidatesExamined = spatialResult.value().examinedRecords,
                },
        };
        result.backToFront.reserve(spatialResult.value().candidates.size());
        for (ObjectId objectId : spatialResult.value().candidates) {
            const SceneRecord* record = _records.find(objectId);
            if (record != nullptr && isVisible(*record) &&
                record->worldBounds.intersects(request.worldRect)) {
                result.backToFront.push_back(objectId);
            }
        }
        std::sort(result.backToFront.begin(),
                  result.backToFront.end(),
                  [this](ObjectId left, ObjectId right) {
                      const SceneRecord* leftRecord = _records.find(left);
                      const SceneRecord* rightRecord = _records.find(right);
                      return leftRecord->orderKey < rightRecord->orderKey ||
                             (leftRecord->orderKey == rightRecord->orderKey && left < right);
                  });
        result.backToFront.erase(std::unique(result.backToFront.begin(), result.backToFront.end()),
                                 result.backToFront.end());
        result.diagnostics.visibleRecords = result.backToFront.size();
        return foundation::Result<SceneQueryResult>::success(std::move(result));
    } catch (const std::bad_alloc&) {
        return foundation::Result<SceneQueryResult>::failure(
            makeError(foundation::ErrorCode::kOutOfMemory, "Scene query could not allocate"));
    }
}

foundation::Result<HitTestResult> Scene::hitTest(const HitTestRequest& request) const {
    if (_publicationGate != nullptr && _publicationGate->transactionActive) {
        return foundation::Result<HitTestResult>::failure(makeError(
            foundation::ErrorCode::kParticipantRejected,
            "HitTest deferred while Scene publication is active"));
    }
    if (!std::isfinite(request.worldPoint.x) || !std::isfinite(request.worldPoint.y) ||
        !std::isfinite(request.tolerance) || request.tolerance < 0.0F ||
        request.maximumResults == 0U || !hasKnownHitKinds(request.filter.kinds)) {
        return foundation::Result<HitTestResult>::failure(
            makeError(foundation::ErrorCode::kInvalidArgument, "HitTest request is invalid"));
    }
    const float epsilon = std::numeric_limits<float>::epsilon() *
                          std::max({1.0F,
                                    std::fabs(request.worldPoint.x),
                                    std::fabs(request.worldPoint.y),
                                    request.tolerance});
    const float radius = request.tolerance + epsilon;
    const WorldRect coarseRect{
        request.worldPoint.x - radius,
        request.worldPoint.y - radius,
        request.worldPoint.x + radius,
        request.worldPoint.y + radius,
    };
    if (!coarseRect.isFiniteAndOrdered()) {
        return foundation::Result<HitTestResult>::failure(
            makeError(foundation::ErrorCode::kInvalidArgument, "HitTest query bounds overflow"));
    }
    auto spatialResult = _spatialIndex->query(coarseRect);
    if (!spatialResult) {
        return foundation::Result<HitTestResult>::failure(spatialResult.error());
    }
    try {
        HitTestResult result{
            .revision = _revision,
            .frontToBack = {},
            .diagnostics = {},
        };
        result.diagnostics.candidatesExamined = spatialResult.value().examinedRecords;
        std::vector<ObjectId> candidates;
        candidates.reserve(spatialResult.value().candidates.size());
        std::unordered_set<ObjectId, foundation::ObjectIdHash> seen;
        seen.reserve(spatialResult.value().candidates.size());
        for (ObjectId objectId : spatialResult.value().candidates) {
            if (!seen.insert(objectId).second) {
                continue;
            }
            const SceneRecord* record = _records.find(objectId);
            if (record == nullptr || !hasFlag(record->flags, SceneRecordFlags::kVisible) ||
                !hasFlag(record->flags, SceneRecordFlags::kHitTestable) ||
                !record->worldBounds.intersects(coarseRect) ||
                (!request.filter.includeLocked &&
                 hasFlag(record->flags, SceneRecordFlags::kLocked)) ||
                (kindBit(record->kind) & static_cast<std::uint32_t>(request.filter.kinds)) == 0U) {
                continue;
            }
            candidates.push_back(objectId);
        }
        result.diagnostics.candidatesReturned = spatialResult.value().candidates.size();
        result.diagnostics.candidatesAfterFilter = candidates.size();
        std::sort(candidates.begin(), candidates.end(), [this](ObjectId left, ObjectId right) {
            const SceneRecord* leftRecord = _records.find(left);
            const SceneRecord* rightRecord = _records.find(right);
            return rightRecord->orderKey < leftRecord->orderKey ||
                   (rightRecord->orderKey == leftRecord->orderKey && right < left);
        });
        for (ObjectId objectId : candidates) {
            ++result.diagnostics.preciseTests;
            auto precise = _renderScene->preciseHitTest(
                PreciseHitRequest{objectId, request.worldPoint, request.tolerance});
            if (!precise) {
                return foundation::Result<HitTestResult>::failure(precise.error());
            }
            if (!precise.value().hit) {
                continue;
            }
            result.frontToBack.push_back(objectId);
            ++result.diagnostics.preciseHits;
            if (result.frontToBack.size() >= request.maximumResults) {
                break;
            }
        }
        return foundation::Result<HitTestResult>::success(std::move(result));
    } catch (const std::bad_alloc&) {
        return foundation::Result<HitTestResult>::failure(
            makeError(foundation::ErrorCode::kOutOfMemory, "HitTest could not allocate"));
    }
}

foundation::Result<SceneDrawList> Scene::buildDrawList(const SceneQueryResult& visible) const {
    if (_publicationGate != nullptr && _publicationGate->transactionActive) {
        return foundation::Result<SceneDrawList>::failure(makeError(
            foundation::ErrorCode::kParticipantRejected,
            "Draw-list construction deferred while Scene publication is active"));
    }
    if (visible.revision != _revision) {
        return foundation::Result<SceneDrawList>::failure(
            makeError(foundation::ErrorCode::kInvalidRevision, "Scene query result is stale"));
    }
    return _renderScene->buildDrawList(visible.backToFront);
}

foundation::Result<SceneFrameInput> Scene::buildFrame(const SceneQuery& request,
                                                       SceneRevision afterExclusive) const {
    if (_publicationGate != nullptr && _publicationGate->transactionActive) {
        return foundation::Result<SceneFrameInput>::failure(makeError(
            foundation::ErrorCode::kParticipantRejected,
            "Frame construction deferred while Scene publication is active"));
    }
    if (afterExclusive > _revision) {
        return foundation::Result<SceneFrameInput>::failure(makeError(
            foundation::ErrorCode::kInvalidRevision,
            "Frame damage revision cannot be newer than the current Scene"));
    }
    auto queryResult = query(request);
    if (!queryResult) {
        return foundation::Result<SceneFrameInput>::failure(queryResult.error());
    }
    auto drawListResult = buildDrawList(queryResult.value());
    if (!drawListResult) {
        return foundation::Result<SceneFrameInput>::failure(drawListResult.error());
    }
    SceneFrameInput frame{
        .revision = _revision,
        .damage = collectDamage(afterExclusive, _revision),
        .query = std::move(queryResult.value()),
        .drawList = std::move(drawListResult.value()),
    };
    return foundation::Result<SceneFrameInput>::success(std::move(frame));
}

DamageSet Scene::collectDamage(SceneRevision afterExclusive, SceneRevision throughInclusive) const {
    return _damageTracker.collect(afterExclusive, throughInclusive);
}

void Scene::compactDamageThrough(SceneRevision revision) {
    _damageTracker.compactThrough(revision);
}

} // namespace canvas
