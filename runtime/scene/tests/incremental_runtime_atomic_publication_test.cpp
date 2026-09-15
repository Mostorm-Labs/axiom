#include "canvas/scene/incremental_runtime_coordinator.hpp"
#include "canvas/scene/testing/fake_render_scene.hpp"
#include "canvas/scene/testing/fake_spatial_index.hpp"
#include "canvas/semantic/reference_object_store.hpp"
#include "incremental_runtime_test_access.hpp"

#include <cstdlib>
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
    canvas::IncrementalRuntimeTestAccess::failAt(
        coordinator, canvas::RuntimeCheckpoint::kBeforePublication);
    renderRaw->setRejectPrepare(false);
    const canvas::SceneCommitInput input(
        canvas::semantic::SemanticGeneration(1), canvas::semantic::SemanticGeneration(2), view,
        &changes);
    const auto result = coordinator.apply(Compiler{}, input);
    const bool checkpointAtomic =
        !result && result.error().code == canvas::foundation::ErrorCode::kParticipantRejected &&
                   scene.revision() == beforeRevision && renderRaw->stateDigest() == beforeDigest &&
                   spatialRaw->stateDigest() == beforeSpatialDigest &&
                   coordinator.runtimeScene().generation() == beforeRuntimeGeneration;
    canvas::IncrementalRuntimeTestAccess::clear(coordinator);
    if (!checkpointAtomic) return EXIT_FAILURE;

    // Exercise the legacy participant failure independently from the
    // coordinator checkpoint seam.  The render rejection must happen during
    // prepare, before any participant is published.
    renderRaw->setRejectPrepare(true);
    const auto rejected = coordinator.apply(Compiler{}, input);
    return !rejected && rejected.error().code == canvas::foundation::ErrorCode::kParticipantRejected &&
                   scene.revision() == beforeRevision &&
                   coordinator.runtimeScene().generation() == beforeRuntimeGeneration
               ? EXIT_SUCCESS : EXIT_FAILURE;
}
