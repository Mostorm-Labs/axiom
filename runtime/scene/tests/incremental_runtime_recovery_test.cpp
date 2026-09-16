#include "canvas/scene/incremental_runtime_coordinator.hpp"
#include "canvas/scene/testing/fake_render_scene.hpp"
#include "canvas/scene/testing/fake_spatial_index.hpp"
#include "canvas/semantic/reference_object_store.hpp"
#include "incremental_runtime_full_oracle_adapter.hpp"
#include "incremental_runtime_test_access.hpp"
#include "canvas/scene/bounds_system.hpp"
#include "../../semantic/src/object_store_mutator.hpp"

#include <algorithm>
#include <cstdlib>
#include <memory>
#include <vector>

namespace {
canvas::semantic::ObjectRecord semanticObject(std::uint64_t id, double tx) {
    canvas::semantic::ObjectRecord object;
    object.id = canvas::semantic::ObjectId::fromUint64(id);
    object.kind = canvas::semantic::ObjectKind::kShape;
    object.kind_version = 1;
    object.content = canvas::semantic::ShapeContent{1, 10.0, 10.0};
    object.transform.tx = tx;
    return object;
}

canvas::SceneRecord sceneRecord(const canvas::semantic::ObjectRecord& object,
                                canvas::SceneRevision revision) {
    const auto bounds = canvas::computeBounds(object);
    return canvas::SceneRecord{
        .objectId = object.id,
        .orderKey = canvas::SceneOrderKey(object.id.bytes[0]),
        .flags = canvas::SceneRecordFlags::kVisible,
        .worldBounds = bounds.world,
        .contentRevision = canvas::ContentRevision(revision.value()),
        .renderPayload = canvas::RenderPayloadRef{static_cast<std::uint32_t>(object.id.bytes[0]),
                                                   static_cast<std::uint32_t>(revision.value())},
        .hitGeometry = canvas::HitGeometryRef{static_cast<std::uint32_t>(object.id.bytes[0]),
                                               static_cast<std::uint32_t>(revision.value())},
    };
}

struct RecoveryObserver final {
    canvas::IncrementalRuntimeCoordinator* coordinator = nullptr;
    canvas::Scene* scene = nullptr;
    canvas::semantic::SemanticGeneration goldRuntimeGeneration{};
    canvas::SceneRevision goldSceneRevision{};
    canvas::semantic::SemanticGeneration goldSemanticGeneration{};
    canvas::RuntimeSceneRecord goldRuntime{};
    canvas::SceneRecord goldScene{};
    canvas::WorldRect goldBounds{};
    canvas::SceneInvalidationOutput goldInvalidation{};
    std::vector<canvas::ObjectId> goldQuery;
    bool called = false;
    bool passed = false;
};

void observeRecoveryGold(void* opaque) noexcept {
    auto* observer = static_cast<RecoveryObserver*>(opaque);
    observer->called = true;
    const auto runtime = observer->coordinator->runtimeScene();
    const auto read = observer->scene->read();
    const auto query = observer->scene->query(
        canvas::SceneQuery{canvas::WorldRect{-1000.0F, -1000.0F, 1000.0F, 1000.0F}});
    const auto& invalidation = observer->scene->invalidationOutput();
    const auto* runtimeGold = runtime.find(observer->goldRuntime.objectId);
    const auto* sceneGold = read.find(observer->goldScene.objectId);
    bool queryMatches = false;
    if (query) {
        auto ids = query.value().backToFront;
        std::sort(ids.begin(), ids.end());
        queryMatches = ids == observer->goldQuery;
    }
    bool invalidationMatches = invalidation.generation == observer->goldInvalidation.generation &&
                               invalidation.fullScene == observer->goldInvalidation.fullScene &&
                               invalidation.rects.size() == observer->goldInvalidation.rects.size();
    if (invalidationMatches) {
        for (std::size_t i = 0; i < invalidation.rects.size(); ++i) {
            invalidationMatches = invalidation.rects[i].worldRect ==
                                      observer->goldInvalidation.rects[i].worldRect &&
                                  invalidation.rects[i].reasons ==
                                      observer->goldInvalidation.rects[i].reasons;
            if (!invalidationMatches) break;
        }
    }
    observer->passed = runtime.generation() == observer->goldRuntimeGeneration &&
                       runtime.records().size() == 1U && runtimeGold != nullptr &&
                       *runtimeGold == observer->goldRuntime &&
                       observer->scene->revision() == observer->goldSceneRevision &&
                       observer->scene->semanticGeneration() == observer->goldSemanticGeneration &&
                       read.revision() == observer->goldSceneRevision &&
                       read.records().size() == 1U && sceneGold != nullptr &&
                       *sceneGold == observer->goldScene && queryMatches &&
                       observer->scene->publishedBounds() == observer->goldBounds &&
                       observer->scene->invalidationGeneration() == observer->goldInvalidation.generation &&
                       invalidationMatches;
}

class Compiler final : public canvas::ISemanticSceneCompiler {
  public:
    canvas::foundation::Result<canvas::CompiledSceneSnapshot> compileFull(
        const canvas::semantic::SemanticReadView& view) const override {
        std::vector<canvas::SceneRecord> records;
        for (const auto& object : view.allObjects()) {
            records.push_back(sceneRecord(
                object, canvas::SceneRevision(view.generation().value())));
        }
        return canvas::foundation::Result<canvas::CompiledSceneSnapshot>::success(
            canvas::CompiledSceneSnapshot{canvas::SceneRevision(view.generation().value()),
                                          std::move(records)});
    }
    canvas::foundation::Result<canvas::CompiledSceneDelta> compileDelta(
        const canvas::semantic::SemanticReadView&,
        const canvas::semantic::ChangeSet&) const override {
        return canvas::foundation::Result<canvas::CompiledSceneDelta>::failure(
            {canvas::foundation::ErrorCode::kRequiresFullRebuild, "recovery test"});
    }
};

} // namespace

int main() {
    canvas::semantic::ReferenceObjectStore store;
    canvas::semantic::ReferenceObjectStore goldStore;
    canvas::semantic::ReferenceObjectStore gnewStore;
    const auto goldObject = semanticObject(301, -100.0);
    const auto gnewObject = semanticObject(302, 200.0);
    if (!canvas::semantic::internal::ObjectStoreMutator::insertFresh(goldStore, goldObject) ||
        !canvas::semantic::internal::ObjectStoreMutator::insertFresh(gnewStore, gnewObject)) {
        return EXIT_FAILURE;
    }
    const canvas::semantic::SemanticReadView goldView(
        goldStore, canvas::semantic::SemanticGeneration(1));
    const canvas::semantic::SemanticReadView view(
        gnewStore, canvas::semantic::SemanticGeneration(2));
    canvas::Scene scene(std::make_unique<canvas::testing::FakeRenderScene>(),
                        std::make_unique<canvas::testing::FakeSpatialIndex>());
    canvas::SceneBinding binding(scene);
    canvas::IncrementalRuntimeCoordinator coordinator(binding);
    const canvas::SceneCommitInput seedInput(
        canvas::semantic::SemanticGeneration(1), goldView);
    const auto seedResult = coordinator.recover(Compiler{}, seedInput);
    if (!seedResult || scene.revision() != canvas::SceneRevision(1) ||
        coordinator.runtimeScene().find(canvas::semantic::ObjectId::fromUint64(301)) == nullptr) {
        return EXIT_FAILURE;
    }
    RecoveryObserver observer;
    observer.coordinator = &coordinator;
    observer.scene = &scene;
    observer.goldRuntimeGeneration = coordinator.runtimeScene().generation();
    observer.goldSceneRevision = scene.revision();
    observer.goldSemanticGeneration = scene.semanticGeneration();
    observer.goldRuntime = *coordinator.runtimeScene().find(
        canvas::semantic::ObjectId::fromUint64(301));
    observer.goldScene = *scene.read().find(canvas::ObjectId::fromUint64(301));
    observer.goldBounds = scene.publishedBounds();
    observer.goldInvalidation = scene.invalidationOutput();
    const auto goldQuery = scene.query(
        canvas::SceneQuery{canvas::WorldRect{-1000.0F, -1000.0F, 1000.0F, 1000.0F}});
    if (!goldQuery) return EXIT_FAILURE;
    observer.goldQuery = goldQuery.value().backToFront;
    std::sort(observer.goldQuery.begin(), observer.goldQuery.end());
    canvas::IncrementalRuntimeTestAccess::observe(
        coordinator, observeRecoveryGold, &observer);
    const canvas::SceneCommitInput input(
        canvas::semantic::SemanticGeneration(2), view);
    const auto result = coordinator.recover(Compiler{}, input);
    canvas::IncrementalRuntimeTestAccess::observe(coordinator, nullptr, nullptr);
    const auto oracle = canvas::testing::compileFullOracle(view);
    if (!result || result.value().disposition != canvas::SceneSyncDisposition::kRebuiltFull ||
        scene.revision() != canvas::SceneRevision(2) || !oracle.valid ||
        oracle.generation != canvas::semantic::SemanticGeneration(2) ||
        oracle.records.empty() || !observer.called || !observer.passed ||
        coordinator.runtimeScene().find(canvas::semantic::ObjectId::fromUint64(301)) != nullptr ||
        coordinator.runtimeScene().find(canvas::semantic::ObjectId::fromUint64(302)) == nullptr) {
        return EXIT_FAILURE;
    }
    const auto expectedGnewScene = sceneRecord(gnewObject, canvas::SceneRevision(2));
    const auto expectedGnewBounds = canvas::computeBounds(gnewObject).world;
    const auto expectedRecoveryDamage = canvas::foundation::unionRects(
        canvas::computeBounds(goldObject).world, expectedGnewBounds);
    const auto gnewRead = scene.read();
    const auto gnewQuery = scene.query(
        canvas::SceneQuery{canvas::WorldRect{-1000.0F, -1000.0F, 1000.0F, 1000.0F}});
    const auto& gnewInvalidation = scene.invalidationOutput();
    if (coordinator.runtimeScene().records().size() != 1U ||
        gnewRead.records().size() != 1U ||
        gnewRead.find(expectedGnewScene.objectId) == nullptr ||
        *gnewRead.find(expectedGnewScene.objectId) != expectedGnewScene ||
        !gnewQuery || gnewQuery.value().backToFront !=
                          std::vector<canvas::ObjectId>{expectedGnewScene.objectId} ||
        scene.publishedBounds() != expectedGnewBounds ||
        scene.invalidationGeneration() != canvas::SceneRevision(2) ||
        gnewInvalidation.generation != canvas::SceneRevision(2) ||
        !gnewInvalidation.fullScene || gnewInvalidation.rects.size() != 1U ||
        gnewInvalidation.rects.front().worldRect != expectedRecoveryDamage ||
        gnewInvalidation.rects.front().reasons != canvas::DamageReason::kFullRebuild) {
        return EXIT_FAILURE;
    }
    const auto goldRevision = canvas::SceneRevision(2);
    const auto goldGeneration = canvas::semantic::SemanticGeneration(2);
    canvas::IncrementalRuntimeTestAccess::failAt(
        coordinator, canvas::RuntimeCheckpoint::kBeforePublication);
    const auto failedBeforePublish = coordinator.recover(Compiler{}, input);
    canvas::IncrementalRuntimeTestAccess::clear(coordinator);
    if (failedBeforePublish || scene.revision() != goldRevision ||
        scene.semanticGeneration() != canvas::semantic::SemanticGeneration(2) ||
        coordinator.runtimeScene().generation() != goldGeneration) {
        return EXIT_FAILURE;
    }
    // A semantic generation gap enters the frozen full-recovery policy.
    const auto gapChanges = canvas::semantic::ChangeSet::fromChanges(
        canvas::semantic::SemanticGeneration(1), canvas::semantic::SemanticGeneration(3), {});
    const canvas::semantic::SemanticReadView gapView(
        store, canvas::semantic::SemanticGeneration(3));
    const canvas::SceneCommitInput gapInput(
        canvas::semantic::SemanticGeneration(1),
        canvas::semantic::SemanticGeneration(3), gapView, &gapChanges);
    const auto gap = coordinator.apply(Compiler{}, gapInput);
    if (!gap) return EXIT_FAILURE;
    if (gap.value().disposition != canvas::SceneSyncDisposition::kRebuiltFull ||
        coordinator.runtimeScene().generation() != canvas::semantic::SemanticGeneration(3) ||
        scene.revision() != canvas::SceneRevision(3)) {
        return EXIT_FAILURE;
    }
    auto gapRender = std::make_unique<canvas::testing::FakeRenderScene>();
    auto* gapRenderRaw = gapRender.get();
    canvas::Scene gapFailureScene(std::move(gapRender),
                                  std::make_unique<canvas::testing::FakeSpatialIndex>());
    canvas::SceneBinding gapFailureBinding(gapFailureScene);
    canvas::IncrementalRuntimeCoordinator gapFailureCoordinator(gapFailureBinding);
    const canvas::semantic::SemanticReadView gapSeedView(
        store, canvas::semantic::SemanticGeneration(1));
    if (!gapFailureCoordinator.recover(
            Compiler{}, canvas::SceneCommitInput(canvas::semantic::SemanticGeneration(1), gapSeedView))) {
        return EXIT_FAILURE;
    }
    gapRenderRaw->setRejectPrepare(true);
    const auto failedGap = gapFailureCoordinator.apply(Compiler{}, gapInput);
    if (failedGap || gapFailureScene.revision() != canvas::SceneRevision(1) ||
        gapFailureScene.semanticGeneration() != canvas::semantic::SemanticGeneration(1) ||
        gapFailureCoordinator.runtimeScene().generation() != canvas::semantic::SemanticGeneration(1)) {
        return EXIT_FAILURE;
    }
    gapRenderRaw->setRejectPrepare(false);
    const auto recoveredGap = gapFailureCoordinator.apply(Compiler{}, gapInput);
    if (!recoveredGap || gapFailureScene.revision() != canvas::SceneRevision(3) ||
        gapFailureCoordinator.runtimeScene().generation() != canvas::semantic::SemanticGeneration(3)) {
        return EXIT_FAILURE;
    }
    // A dropped ChangeSet enters the same explicit full-recovery policy.
    canvas::Scene droppedScene(std::make_unique<canvas::testing::FakeRenderScene>(),
                               std::make_unique<canvas::testing::FakeSpatialIndex>());
    canvas::SceneBinding droppedBinding(droppedScene);
    canvas::IncrementalRuntimeCoordinator droppedCoordinator(droppedBinding);
    const auto dropped = droppedCoordinator.apply(Compiler{}, input);
    if (!dropped || dropped.value().disposition != canvas::SceneSyncDisposition::kRebuiltFull ||
        droppedScene.revision() != canvas::SceneRevision(2) ||
        droppedCoordinator.runtimeScene().generation() != canvas::semantic::SemanticGeneration(2)) {
        return EXIT_FAILURE;
    }
    canvas::IncrementalRuntimeTestAccess::corrupt(coordinator);
    if (coordinator.runtimeScene().generation() != canvas::semantic::SemanticGeneration(0)) {
        return EXIT_FAILURE;
    }
    canvas::IncrementalRuntimeTestAccess::failAt(
        coordinator, canvas::RuntimeCheckpoint::kBeforePublication);
    const auto failedRecovery = coordinator.recover(Compiler{}, input);
    if (failedRecovery || coordinator.runtimeScene().generation() != canvas::semantic::SemanticGeneration(0)) {
        return EXIT_FAILURE;
    }
    // A failed participant prepare must not publish either the rebuilt Scene
    // or the canonical RuntimeScene projection.
    auto failingRender = std::make_unique<canvas::testing::FakeRenderScene>();
    auto* failingRenderRaw = failingRender.get();
    failingRenderRaw->setRejectPrepare(true);
    canvas::Scene failingScene(std::move(failingRender),
                               std::make_unique<canvas::testing::FakeSpatialIndex>());
    canvas::SceneBinding failingBinding(failingScene);
    canvas::IncrementalRuntimeCoordinator failingCoordinator(failingBinding);
    const auto rejected = failingCoordinator.recover(Compiler{}, input);
    if (!rejected && rejected.error().code == canvas::foundation::ErrorCode::kParticipantRejected &&
                   failingCoordinator.runtimeScene().generation() == canvas::semantic::SemanticGeneration(0) &&
                   failingScene.revision() == canvas::SceneRevision(0)) {
        return EXIT_SUCCESS;
    }
    return EXIT_FAILURE;
}
