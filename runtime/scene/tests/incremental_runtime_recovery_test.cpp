#include "canvas/scene/incremental_runtime_coordinator.hpp"
#include "canvas/scene/testing/fake_render_scene.hpp"
#include "canvas/scene/testing/fake_spatial_index.hpp"
#include "canvas/semantic/reference_object_store.hpp"
#include "incremental_runtime_full_oracle_adapter.hpp"
#include "incremental_runtime_test_access.hpp"

#include <cstdlib>
#include <memory>

namespace {
class Compiler final : public canvas::ISemanticSceneCompiler {
  public:
    canvas::foundation::Result<canvas::CompiledSceneSnapshot> compileFull(
        const canvas::semantic::SemanticReadView& view) const override {
        return canvas::foundation::Result<canvas::CompiledSceneSnapshot>::success(
            canvas::CompiledSceneSnapshot{canvas::SceneRevision(view.generation().value()), {}});
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
    const canvas::semantic::SemanticReadView view(
        store, canvas::semantic::SemanticGeneration(2));
    canvas::Scene scene(std::make_unique<canvas::testing::FakeRenderScene>(),
                        std::make_unique<canvas::testing::FakeSpatialIndex>());
    canvas::SceneBinding binding(scene);
    canvas::IncrementalRuntimeCoordinator coordinator(binding);
    const canvas::SceneCommitInput input(
        canvas::semantic::SemanticGeneration(2), view);
    const auto result = coordinator.recover(Compiler{}, input);
    const auto oracle = canvas::testing::compileFullOracle(view);
    if (!result || result.value().disposition != canvas::SceneSyncDisposition::kRebuiltFull ||
        scene.revision() != canvas::SceneRevision(2) || !oracle.valid || oracle.generation != canvas::semantic::SemanticGeneration(2) ||
        !oracle.records.empty() ||
        coordinator.runtimeScene().generation() != canvas::semantic::SemanticGeneration(2)) {
        return EXIT_FAILURE;
    }
    const auto goldRevision = scene.revision();
    const auto goldGeneration = coordinator.runtimeScene().generation();
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
