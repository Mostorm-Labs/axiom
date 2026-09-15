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
        const canvas::semantic::SemanticReadView&) const override {
        return canvas::foundation::Result<canvas::CompiledSceneSnapshot>::success(
            canvas::CompiledSceneSnapshot{canvas::SceneRevision(2), {}});
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
    return !rejected && rejected.error().code == canvas::foundation::ErrorCode::kParticipantRejected &&
                   failingCoordinator.runtimeScene().generation() == canvas::semantic::SemanticGeneration(0) &&
                   failingScene.revision() == canvas::SceneRevision(0)
               ? EXIT_SUCCESS : EXIT_FAILURE;
}
