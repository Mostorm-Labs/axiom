#include "canvas/scene/incremental_runtime_coordinator.hpp"
#include "canvas/scene/testing/fake_render_scene.hpp"
#include "canvas/scene/testing/fake_spatial_index.hpp"
#include "canvas/semantic/reference_object_store.hpp"
#include "canvas/scene/bounds_system.hpp"
#include "incremental_runtime_test_access.hpp"
#include "../../semantic/src/object_store_mutator.hpp"

#include <cstdlib>
#include <array>
#include <optional>
#include <vector>

namespace {
canvas::SceneRecord record(std::uint64_t id) {
    return canvas::SceneRecord{
        .objectId = canvas::ObjectId::fromUint64(id),
        .orderKey = canvas::SceneOrderKey(id),
        .flags = canvas::SceneRecordFlags::kVisible,
        .worldBounds = canvas::WorldRect{0, 0, 1, 1},
        .contentRevision = canvas::ContentRevision(1),
        .renderPayload = canvas::RenderPayloadRef{static_cast<std::uint32_t>(id), 1},
        .hitGeometry = canvas::HitGeometryRef{static_cast<std::uint32_t>(id), 1},
    };
}

canvas::SceneRecord boundsRecord(std::uint64_t id, canvas::WorldRect bounds) {
    auto value = record(id);
    value.worldBounds = bounds;
    return value;
}

class Compiler final : public canvas::ISemanticSceneCompiler {
  public:
    canvas::foundation::Result<canvas::CompiledSceneSnapshot> compileFull(
        const canvas::semantic::SemanticReadView&) const override {
        return canvas::foundation::Result<canvas::CompiledSceneSnapshot>::success(
            canvas::CompiledSceneSnapshot{canvas::SceneRevision(1), {record(1)}});
    }
    canvas::foundation::Result<canvas::CompiledSceneDelta> compileDelta(
        const canvas::semantic::SemanticReadView&,
        const canvas::semantic::ChangeSet&) const override {
        const auto inserted = record(2);
        return canvas::foundation::Result<canvas::CompiledSceneDelta>::success(
            canvas::CompiledSceneDelta{
                .beforeRevision = canvas::SceneRevision(1),
                .afterRevision = canvas::SceneRevision(2),
                .mutations = {canvas::SceneMutation{
                    .kind = canvas::SceneMutationKind::kInsert,
                    .objectId = inserted.objectId,
                    .before = std::nullopt,
                    .after = inserted,
                }},
                .hints = std::nullopt,
            });
    }
};

class MultiBoundsCompiler final : public canvas::ISemanticSceneCompiler {
  public:
    canvas::foundation::Result<canvas::CompiledSceneSnapshot> compileFull(
        const canvas::semantic::SemanticReadView&) const override {
        return canvas::foundation::Result<canvas::CompiledSceneSnapshot>::success(
            canvas::CompiledSceneSnapshot{canvas::SceneRevision(1), {}});
    }
    canvas::foundation::Result<canvas::CompiledSceneDelta> compileDelta(
        const canvas::semantic::SemanticReadView&,
        const canvas::semantic::ChangeSet&) const override {
        return canvas::foundation::Result<canvas::CompiledSceneDelta>::success(
            canvas::CompiledSceneDelta{
                .beforeRevision = canvas::SceneRevision(1),
                .afterRevision = canvas::SceneRevision(2),
                .mutations = {canvas::SceneMutation{
                                  .kind = canvas::SceneMutationKind::kInsert,
                                  .objectId = canvas::ObjectId::fromUint64(101),
                                  .before = std::nullopt,
                                  .after = boundsRecord(101, canvas::WorldRect{-20.0F, 0.0F, -10.0F, 10.0F})},
                              canvas::SceneMutation{
                                  .kind = canvas::SceneMutationKind::kInsert,
                                  .objectId = canvas::ObjectId::fromUint64(102),
                                  .before = std::nullopt,
                                  .after = boundsRecord(102, canvas::WorldRect{30.0F, 0.0F, 40.0F, 10.0F})}},
                .hints = std::nullopt});
    }
};
} // namespace

int main() {
    canvas::semantic::ReferenceObjectStore store;
    const canvas::semantic::SemanticReadView seedView(
        store, canvas::semantic::SemanticGeneration(1));
    const canvas::semantic::SemanticReadView view(
        store, canvas::semantic::SemanticGeneration(2));
    const auto changes = canvas::semantic::ChangeSet::fromChanges(
        canvas::semantic::SemanticGeneration(1), canvas::semantic::SemanticGeneration(2), {});
    auto render = std::make_unique<canvas::testing::FakeRenderScene>();
    auto* renderRaw = render.get();
    auto spatial = std::make_unique<canvas::testing::FakeSpatialIndex>();
    auto* spatialRaw = spatial.get();
    canvas::Scene scene(std::move(render), std::move(spatial));
    canvas::SceneBinding binding(scene);
    canvas::IncrementalRuntimeCoordinator coordinator(binding);
    const canvas::SceneCommitInput seedInput(
        canvas::semantic::SemanticGeneration(1), seedView);
    if (!coordinator.recover(Compiler{}, seedInput)) return EXIT_FAILURE;
    const auto beforeRevision = scene.revision();
    const auto beforeDigest = renderRaw->stateDigest();
    const auto beforeSpatialDigest = spatialRaw->stateDigest();
    const auto beforeRuntimeGeneration = coordinator.runtimeScene().generation();
    renderRaw->setRejectPrepare(false);
    const canvas::SceneCommitInput input(
        canvas::semantic::SemanticGeneration(1), canvas::semantic::SemanticGeneration(2), view,
        &changes);
    const std::array checkpoints{
        canvas::RuntimeCheckpoint::kBeforeRuntimePrepare,
        canvas::RuntimeCheckpoint::kAfterRuntimePrepare,
        canvas::RuntimeCheckpoint::kBeforeBoundsPrepare,
        canvas::RuntimeCheckpoint::kAfterBoundsPrepare,
        canvas::RuntimeCheckpoint::kBeforeSpatialPrepare,
        canvas::RuntimeCheckpoint::kAfterSpatialPrepare,
        canvas::RuntimeCheckpoint::kBeforeInvalidationFinalization,
        canvas::RuntimeCheckpoint::kAfterInvalidationFinalization,
        canvas::RuntimeCheckpoint::kBeforePublication};
    for (const auto checkpoint : checkpoints) {
        canvas::IncrementalRuntimeTestAccess::failAt(coordinator, checkpoint);
        const auto result = coordinator.apply(Compiler{}, input);
        const bool checkpointAtomic =
            !result && result.error().code == canvas::foundation::ErrorCode::kParticipantRejected &&
                       scene.revision() == beforeRevision && renderRaw->stateDigest() == beforeDigest &&
                       spatialRaw->stateDigest() == beforeSpatialDigest &&
                       coordinator.runtimeScene().generation() == beforeRuntimeGeneration;
        canvas::IncrementalRuntimeTestAccess::clear(coordinator);
        if (!checkpointAtomic) return EXIT_FAILURE;
    }

    // Exercise the legacy participant failure independently from the
    // coordinator checkpoint seam.  The render rejection must happen during
    // prepare, before any participant is published.
    renderRaw->setRejectPrepare(true);
    const auto rejected = coordinator.apply(Compiler{}, input);
    if (rejected || rejected.error().code != canvas::foundation::ErrorCode::kParticipantRejected ||
        scene.revision() != beforeRevision ||
        coordinator.runtimeScene().generation() != beforeRuntimeGeneration) {
        return EXIT_FAILURE;
    }
    const auto recoveredQuery = scene.query(canvas::SceneQuery{canvas::WorldRect{-1.0F, -1.0F, 2.0F, 2.0F}});
    if (!recoveredQuery || recoveredQuery.error().code == canvas::foundation::ErrorCode::kParticipantRejected) {
        return EXIT_FAILURE;
    }
    const auto recoveredHit = scene.hitTest(canvas::HitTestRequest{
        .worldPoint = canvas::WorldPoint{0.0F, 0.0F},
        .tolerance = 0.0F,
        .filter = canvas::HitTestFilter{},
        .maximumResults = 1U,
    });
    if (!recoveredHit || recoveredHit.error().code == canvas::foundation::ErrorCode::kParticipantRejected) {
        return EXIT_FAILURE;
    }

    canvas::semantic::ObjectRecord objectA;
    objectA.id = canvas::semantic::ObjectId::fromUint64(101);
    objectA.kind = canvas::semantic::ObjectKind::kShape;
    objectA.kind_version = 1;
    objectA.content = canvas::semantic::ShapeContent{1, 10.0, 10.0};
    objectA.transform.tx = -20.0;
    canvas::semantic::ObjectRecord objectB = objectA;
    objectB.id = canvas::semantic::ObjectId::fromUint64(102);
    objectB.transform.tx = 30.0;
    const auto boundsA = canvas::computeBounds(objectA);
    const auto boundsB = canvas::computeBounds(objectB);
    const auto expectedUnion = canvas::foundation::unionRects(boundsA.world, boundsB.world);
    if (!boundsA.finite || !boundsB.finite || expectedUnion == boundsA.world ||
        expectedUnion == boundsB.world) {
        return EXIT_FAILURE;
    }
    canvas::semantic::ReferenceObjectStore boundsStore;
    if (!canvas::semantic::internal::ObjectStoreMutator::insertFresh(boundsStore, objectA) ||
        !canvas::semantic::internal::ObjectStoreMutator::insertFresh(boundsStore, objectB)) {
        return EXIT_FAILURE;
    }
    const canvas::semantic::SemanticReadView boundsView(
        boundsStore, canvas::semantic::SemanticGeneration(2));
    const auto boundsChanges = canvas::semantic::ChangeSet::fromChanges(
        canvas::semantic::SemanticGeneration(1), canvas::semantic::SemanticGeneration(2),
        {{objectA.id, canvas::semantic::SemanticChangeFlags::kContent, {}},
         {objectB.id, canvas::semantic::SemanticChangeFlags::kContent, {}}});
    if (boundsChanges.objects().size() != 2U) return EXIT_FAILURE;
    canvas::Scene boundsScene(std::make_unique<canvas::testing::FakeRenderScene>(),
                              std::make_unique<canvas::testing::FakeSpatialIndex>());
    canvas::SceneBinding boundsBinding(boundsScene);
    canvas::IncrementalRuntimeCoordinator boundsCoordinator(boundsBinding);
    const canvas::semantic::SemanticReadView boundsSeedView(
        boundsStore, canvas::semantic::SemanticGeneration(1));
    const canvas::SceneCommitInput boundsSeedInput(
        canvas::semantic::SemanticGeneration(1), boundsSeedView);
    if (!boundsCoordinator.recover(MultiBoundsCompiler{}, boundsSeedInput)) return EXIT_FAILURE;
    const auto oldBounds = boundsScene.publishedBounds();
    const canvas::SceneCommitInput boundsInput(
        canvas::semantic::SemanticGeneration(1), canvas::semantic::SemanticGeneration(2),
        boundsView, &boundsChanges);
    canvas::IncrementalRuntimeTestAccess::failAt(
        boundsCoordinator, canvas::RuntimeCheckpoint::kBeforePublication);
    if (boundsCoordinator.apply(MultiBoundsCompiler{}, boundsInput) ||
        boundsScene.publishedBounds() != oldBounds) {
        return EXIT_FAILURE;
    }
    canvas::IncrementalRuntimeTestAccess::clear(boundsCoordinator);
    if (!boundsCoordinator.apply(MultiBoundsCompiler{}, boundsInput) ||
        boundsScene.publishedBounds() != expectedUnion) {
        return EXIT_FAILURE;
    }

    // A successful transaction invokes observers after each participant commit;
    // every observation must still resolve to the prior published snapshot
    // until the single publication callback closes the epoch.
    renderRaw->setRejectPrepare(false);
    const auto applied = coordinator.apply(Compiler{}, input);
    const auto target = canvas::semantic::SemanticGeneration(2);
    const auto& invalidation = scene.invalidationOutput();
    return applied && coordinator.publicationObservationCoherent() &&
                   scene.semanticGeneration() == target &&
                   scene.invalidationGeneration() == canvas::SceneRevision(2) &&
                   invalidation.generation == canvas::SceneRevision(2) &&
                   !invalidation.rects.empty()
               ? EXIT_SUCCESS
               : EXIT_FAILURE;
}
