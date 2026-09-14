#include "canvas/scene/incremental_runtime_coordinator.hpp"
#include "canvas/scene/testing/fake_render_scene.hpp"
#include "canvas/scene/testing/fake_spatial_index.hpp"
#include "canvas/semantic/reference_object_store.hpp"

#include <cstdlib>
#include <iostream>
#include <memory>
#include <optional>

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
    explicit Compiler(bool failIncremental = false) : failIncremental_(failIncremental) {}
    canvas::foundation::Result<canvas::CompiledSceneSnapshot> compileFull(
        const canvas::semantic::SemanticReadView&) const override {
        return canvas::foundation::Result<canvas::CompiledSceneSnapshot>::success(
            canvas::CompiledSceneSnapshot{canvas::SceneRevision(1), {record(1)}});
    }
    canvas::foundation::Result<canvas::CompiledSceneDelta> compileDelta(
        const canvas::semantic::SemanticReadView&,
        const canvas::semantic::ChangeSet&) const override {
        if (failIncremental_) {
            return canvas::foundation::Result<canvas::CompiledSceneDelta>::failure(
                {canvas::foundation::ErrorCode::kRequiresFullRebuild, "unsafe incremental"});
        }
        const auto before = record(1);
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

  private:
    bool failIncremental_ = false;
};
} // namespace

int main() {
    canvas::semantic::ReferenceObjectStore store;
    const canvas::semantic::SemanticReadView view(store, canvas::semantic::SemanticGeneration(1));
    const auto changes = canvas::semantic::ChangeSet::fromChanges(
        canvas::semantic::SemanticGeneration(1), canvas::semantic::SemanticGeneration(2), {});
    canvas::RuntimeUpdatePlan defaultPlan;
    if (defaultPlan.disposition != canvas::RuntimeUpdateDisposition::kIncremental) {
        std::cerr << "default plan disposition mismatch\n";
        return EXIT_FAILURE;
    }
    canvas::Scene scene(std::make_unique<canvas::testing::FakeRenderScene>(),
                        std::make_unique<canvas::testing::FakeSpatialIndex>());
    canvas::SceneBinding binding(scene);
    canvas::IncrementalRuntimeCoordinator runtime(binding);
    const canvas::SceneCommitInput seedInput(
        canvas::semantic::SemanticGeneration(1), view);
    auto seed = runtime.recover(Compiler{}, seedInput);
    if (!seed || seed.value().disposition != canvas::SceneSyncDisposition::kRebuiltFull ||
        scene.revision() != canvas::SceneRevision(1)) {
        std::cerr << "recovery seed failed\n";
        return EXIT_FAILURE;
    }

    const auto incrementalView = canvas::semantic::SemanticReadView(
        store, canvas::semantic::SemanticGeneration(2));
    const canvas::SceneCommitInput input(
        canvas::semantic::SemanticGeneration(1), canvas::semantic::SemanticGeneration(2),
        incrementalView, &changes);
    auto applied = runtime.apply(Compiler{}, input);
    if (!applied || applied.value().disposition != canvas::SceneSyncDisposition::kAppliedIncremental ||
        scene.revision() != canvas::SceneRevision(2) ||
        scene.read().find(canvas::ObjectId::fromUint64(2)) == nullptr) {
        std::cerr << "incremental publication failed\n";
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
