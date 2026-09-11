#include "canvas/scene/scene_binding.hpp"
#include "canvas/scene/scene_commit_input.hpp"

#include "canvas/scene/scene.hpp"
#include "canvas/scene/testing/fake_render_scene.hpp"
#include "canvas/scene/testing/fake_spatial_index.hpp"
#include "canvas/semantic/reference_object_store.hpp"

#include <cassert>
#include <memory>
#include <type_traits>
#include <vector>

int main() {
    using canvas::CompiledSceneDelta;
    using canvas::CompiledSceneSnapshot;
    using canvas::Scene;
    using canvas::SceneCommitInput;
    using canvas::SceneRevision;
    using canvas::semantic::ChangeSet;
    using canvas::semantic::SemanticGeneration;
    using canvas::semantic::SemanticReadView;

    static_assert(!std::is_copy_constructible_v<SceneCommitInput>);
    static_assert(std::is_same_v<decltype(SceneCommitInput::before_generation),
                                 const SemanticGeneration>);

    canvas::semantic::ReferenceObjectStore objects;
    const SemanticReadView post_state(objects, SemanticGeneration(1));
    const ChangeSet changes = ChangeSet::fromChanges(
        SemanticGeneration(0), SemanticGeneration(1), {});
    SceneCommitInput input(SemanticGeneration(0), SemanticGeneration(1), post_state, &changes);
    assert(input.post_state.generation() == SemanticGeneration(1));

    struct Compiler final : canvas::ISemanticSceneCompiler {
        canvas::foundation::Result<CompiledSceneSnapshot> compileFull(
            const SemanticReadView& view) const override {
            assert(view.generation() == SemanticGeneration(0) ||
                   view.generation() == SemanticGeneration(7));
            return canvas::foundation::Result<CompiledSceneSnapshot>::success(
                CompiledSceneSnapshot{.sourceRevision = SceneRevision(1), .records = {}});
        }

        canvas::foundation::Result<CompiledSceneDelta> compileDelta(
            const SemanticReadView& view,
            const ChangeSet& change_set) const override {
            assert(view.generation() == change_set.afterGeneration());
            return canvas::foundation::Result<CompiledSceneDelta>::success(
                CompiledSceneDelta{.beforeRevision = SceneRevision(1),
                                   .afterRevision = SceneRevision(2),
                                   .mutations = {},
                                   .hints = std::nullopt});
        }
    } compiler;

    Scene initial_scene(std::make_unique<canvas::testing::FakeRenderScene>(),
                        std::make_unique<canvas::testing::FakeSpatialIndex>());
    canvas::SceneBinding initial_binding(initial_scene);
    const SemanticReadView generation_zero(objects, SemanticGeneration(0));
    const SceneCommitInput initial(SemanticGeneration(0), generation_zero);
    const auto initial_receipt = initial_binding.rebuild(compiler, initial);
    assert(initial_receipt.hasValue());
    assert(initial_receipt.value().semanticGeneration == SemanticGeneration(0));

    // A non-zero semantic revision is also valid for a fresh Scene when the
    // first published snapshot already reflects an established document.
    Scene scene(std::make_unique<canvas::testing::FakeRenderScene>(),
                std::make_unique<canvas::testing::FakeSpatialIndex>());
    assert(scene.revision() == SceneRevision(0));
    canvas::SceneBinding binding(scene);
    const SemanticReadView generation_seven(objects, SemanticGeneration(7));
    const SceneCommitInput full(SemanticGeneration(7), generation_seven);
    const auto full_receipt = binding.rebuild(compiler, full);
    assert(full_receipt.hasValue());
    assert(full_receipt.value().semanticGeneration == SemanticGeneration(7));

    const SemanticReadView generation_eight(objects, SemanticGeneration(8));
    const ChangeSet next_changes = ChangeSet::fromChanges(
        SemanticGeneration(7), SemanticGeneration(8), {});
    const SceneCommitInput next(
        SemanticGeneration(7), SemanticGeneration(8), generation_eight, &next_changes);
    const auto next_receipt = binding.synchronize(compiler, next);
    assert(next_receipt.hasValue());
    assert(next_receipt.value().semanticGeneration == SemanticGeneration(8));

    const SemanticReadView stale_view(objects, SemanticGeneration(10));
    const ChangeSet gap_changes = ChangeSet::fromChanges(
        SemanticGeneration(9), SemanticGeneration(10), {});
    const SceneCommitInput gap(
        SemanticGeneration(9), SemanticGeneration(10), stale_view, &gap_changes);
    const auto rejected = binding.synchronize(compiler, gap);
    assert(!rejected.hasValue());
    assert(rejected.error().code == canvas::foundation::ErrorCode::kInvalidRevision);
    assert(scene.semanticGeneration() == SemanticGeneration(8));

    const SemanticReadView mismatched_view(objects, SemanticGeneration(9));
    const ChangeSet mismatched_changes = ChangeSet::fromChanges(
        SemanticGeneration(8), SemanticGeneration(9), {});
    const SceneCommitInput mismatched(
        SemanticGeneration(8), SemanticGeneration(9), mismatched_view, &mismatched_changes);
    const auto mismatched_result = binding.synchronize(compiler, mismatched);
    assert(!mismatched_result.hasValue());
    assert(mismatched_result.error().code == canvas::foundation::ErrorCode::kInvalidRevision);
    assert(scene.semanticGeneration() == SemanticGeneration(8));
    assert(scene.revision() == SceneRevision(2));
    return 0;
}
