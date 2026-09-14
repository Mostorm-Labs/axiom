#include "canvas/scene/incremental_runtime_coordinator.hpp"
#include "canvas/scene/testing/fake_render_scene.hpp"
#include "canvas/scene/testing/fake_spatial_index.hpp"
#include "canvas/semantic/reference_object_store.hpp"

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
    return result && result.value().disposition == canvas::SceneSyncDisposition::kRebuiltFull &&
                   scene.revision() == canvas::SceneRevision(2)
               ? EXIT_SUCCESS
               : EXIT_FAILURE;
}
