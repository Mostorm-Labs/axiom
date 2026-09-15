#include "canvas/scene/incremental_runtime_coordinator.hpp"
#include "canvas/scene/testing/fake_render_scene.hpp"
#include "canvas/scene/testing/fake_spatial_index.hpp"
#include "canvas/semantic/reference_object_store.hpp"
#include "incremental_runtime_full_oracle_adapter.hpp"
#include "object_store_mutator.hpp"

#include <cstdlib>
#include <iostream>
#include <memory>
#include <optional>
#include <vector>

// The independent FullSceneCompiler projection family is intentionally kept
// out of this translation unit; its test-only adapter is exercised by the
// standalone A6 oracle target once the production name collision is resolved.

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

canvas::semantic::ObjectRecord semanticRecord(canvas::semantic::ObjectKind kind,
                                              std::uint64_t id,
                                              std::uint64_t seed) {
    canvas::semantic::ObjectRecord object;
    object.id = canvas::semantic::ObjectId::fromUint64(id);
    object.kind = kind;
    object.kind_version = 1;
    object.placement.order_key = canvas::semantic::OrderKey(
        std::vector<std::uint8_t>{static_cast<std::uint8_t>((id + seed) & 0xffU)});
    object.transform.tx = static_cast<double>((id + seed) % 17U);
    switch (kind) {
    case canvas::semantic::ObjectKind::kShape:
        object.content = canvas::semantic::ShapeContent{1, 10.0, 20.0}; break;
    case canvas::semantic::ObjectKind::kImage:
        object.content = canvas::semantic::ImageContent{.width = 11.0, .height = 12.0}; break;
    case canvas::semantic::ObjectKind::kVectorPath:
        object.content = canvas::semantic::VectorPathContent{}; break;
    case canvas::semantic::ObjectKind::kRichText:
        object.content = canvas::semantic::RichTextContent{}; break;
    case canvas::semantic::ObjectKind::kVectorStroke:
        object.content = canvas::semantic::VectorStrokeContent{}; break;
    case canvas::semantic::ObjectKind::kDabStroke:
        object.content = canvas::semantic::DabStrokeContent{}; break;
    case canvas::semantic::ObjectKind::kConnector:
        object.content = canvas::semantic::ConnectorContent{}; break;
    case canvas::semantic::ObjectKind::kSticky:
        object.content = canvas::semantic::StickyContent{13.0, 14.0}; break;
    case canvas::semantic::ObjectKind::kGroup:
        object.content = canvas::semantic::GroupContent{}; break;
    }
    return object;
}

bool equivalent(const canvas::RuntimeSceneRecord& actual,
                const canvas::testing::FullOracleRecord& expected) {
    return actual.objectId == expected.objectId && actual.kind == expected.kind &&
           actual.kindVersion == expected.kindVersion && actual.placement == expected.placement &&
           actual.transform == expected.transform && actual.properties == expected.properties &&
           actual.content == expected.content && actual.eraseMasks == expected.eraseMasks;
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
        const canvas::semantic::ChangeSet& changes) const override {
        if (failIncremental_) {
            return canvas::foundation::Result<canvas::CompiledSceneDelta>::failure(
                {canvas::foundation::ErrorCode::kRequiresFullRebuild, "unsafe incremental"});
        }
        const auto beforeRevision = canvas::SceneRevision(changes.beforeGeneration().value());
        const auto afterRevision = canvas::SceneRevision(changes.afterGeneration().value());
        return canvas::foundation::Result<canvas::CompiledSceneDelta>::success(
            canvas::CompiledSceneDelta{
                .beforeRevision = beforeRevision,
                .afterRevision = afterRevision,
                .mutations = {},
                .hints = std::nullopt,
            });
    }

  private:
    bool failIncremental_ = false;
};
} // namespace

int main() {
    for (std::uint64_t seed = 11; seed <= 13; ++seed) {
        canvas::semantic::ReferenceObjectStore store;
        for (std::uint64_t id = 1; id <= 9; ++id) {
            if (!canvas::semantic::internal::ObjectStoreMutator::insertFresh(
                    store, semanticRecord(static_cast<canvas::semantic::ObjectKind>(id), id, seed))) {
                return EXIT_FAILURE;
            }
        }
        const canvas::semantic::SemanticReadView view(store, canvas::semantic::SemanticGeneration(1));
        const auto oracle = canvas::testing::compileFullOracle(view);
        if (!oracle.valid || oracle.records.size() != 9) return EXIT_FAILURE;
        canvas::Scene scene(std::make_unique<canvas::testing::FakeRenderScene>(),
                            std::make_unique<canvas::testing::FakeSpatialIndex>());
        canvas::SceneBinding binding(scene);
        canvas::IncrementalRuntimeCoordinator runtime(binding);
        const canvas::SceneCommitInput seedInput(canvas::semantic::SemanticGeneration(1), view);
        if (!runtime.recover(Compiler{}, seedInput)) return EXIT_FAILURE;
        for (std::uint64_t step = 2; step <= 1001; ++step) {
            const auto before = canvas::semantic::SemanticGeneration(step - 1);
            const auto after = canvas::semantic::SemanticGeneration(step);
            const auto changes = canvas::semantic::ChangeSet::fromChanges(before, after, {});
            const auto incrementalView = canvas::semantic::SemanticReadView(store, after);
            const canvas::SceneCommitInput input(before, after, incrementalView, &changes);
            const auto applied = runtime.apply(Compiler{}, input);
            if (!applied || applied.value().disposition != canvas::SceneSyncDisposition::kAppliedIncremental) {
                std::cerr << "R07 apply failure seed=" << seed << " step=" << step;
                if (!applied) std::cerr << " code=" << static_cast<int>(applied.error().code) << " msg=" << applied.error().message;
                std::cerr << "\n";
                return EXIT_FAILURE;
            }
            const auto reference = canvas::testing::compileFullOracle(incrementalView);
            if (!reference.valid || reference.generation != after ||
                reference.records.size() != runtime.runtimeScene().records().size()) {
                std::cerr << "R07 mismatch seed=" << seed << " step=" << step << "\n";
                return EXIT_FAILURE;
            }
            for (const auto& expected : reference.records) {
                const auto* actual = runtime.runtimeScene().find(expected.objectId);
                if (actual == nullptr || !equivalent(*actual, expected)) {
                    std::cerr << "R07 record divergence seed=" << seed << " step=" << step
                              << " object_byte0=" << static_cast<unsigned>(expected.objectId.bytes[0]) << "\n";
                    return EXIT_FAILURE;
                }
            }
        }
    }

    // Incremental staging must consume the canonical ChangeSet and preserve
    // unaffected published records instead of rebuilding from the entire view.
    {
        canvas::semantic::ReferenceObjectStore store;
        const auto first = semanticRecord(canvas::semantic::ObjectKind::kShape, 1, 41);
        const auto second = semanticRecord(canvas::semantic::ObjectKind::kImage, 2, 41);
        if (!canvas::semantic::internal::ObjectStoreMutator::insertFresh(store, first) ||
            !canvas::semantic::internal::ObjectStoreMutator::insertFresh(store, second)) {
            return EXIT_FAILURE;
        }
        const auto seedView = canvas::semantic::SemanticReadView(
            store, canvas::semantic::SemanticGeneration(1));
        const auto view = canvas::semantic::SemanticReadView(
            store, canvas::semantic::SemanticGeneration(2));
        const auto changes = canvas::semantic::ChangeSet::fromChanges(
            canvas::semantic::SemanticGeneration(1), canvas::semantic::SemanticGeneration(2),
            {canvas::semantic::ObjectSemanticChange{
                .object_id = second.id,
                .flags = canvas::semantic::SemanticChangeFlags::kTransform,
                .changed_fields = {7U}}});
        canvas::Scene scene(std::make_unique<canvas::testing::FakeRenderScene>(),
                            std::make_unique<canvas::testing::FakeSpatialIndex>());
        canvas::SceneBinding binding(scene);
        canvas::IncrementalRuntimeCoordinator runtime(binding);
        const canvas::SceneCommitInput seedInput(
            canvas::semantic::SemanticGeneration(1), seedView);
        if (!runtime.recover(Compiler{}, seedInput)) return EXIT_FAILURE;
        const auto before = runtime.runtimeScene().find(first.id);
        if (before == nullptr) return EXIT_FAILURE;
        const auto beforeId = before->objectId;
        const auto input = canvas::SceneCommitInput(
            canvas::semantic::SemanticGeneration(1),
            canvas::semantic::SemanticGeneration(2), view, &changes);
        const auto applied = runtime.apply(Compiler{}, input);
        if (!applied || runtime.runtimeScene().generation() != canvas::semantic::SemanticGeneration(2) ||
            runtime.runtimeScene().find(first.id) == nullptr ||
            runtime.runtimeScene().find(first.id)->objectId != beforeId) {
            return EXIT_FAILURE;
        }
    }
    return EXIT_SUCCESS;
}
