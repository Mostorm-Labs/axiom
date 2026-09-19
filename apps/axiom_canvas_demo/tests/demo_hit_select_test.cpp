#include "common/demo_hit_select.hpp"

#include "canvas/scene/direct_render_scene.hpp"
#include "canvas/scene/scene.hpp"
#include "canvas/scene/uniform_grid_spatial_index.hpp"
#include "canvas/semantic/semantic_read_view.hpp"

#include <cassert>
#include <memory>

namespace {

using axiom::demo::CameraController;
using axiom::demo::DemoHitSelectHarness;
using axiom::demo::SceneHitTestPort;
using canvas::CompiledSceneSnapshot;
using canvas::ContentRevision;
using canvas::DirectRenderScene;
using canvas::HitGeometryRef;
using canvas::ObjectId;
using canvas::RenderPayloadRef;
using canvas::RuntimeScene;
using canvas::Scene;
using canvas::SceneObjectKind;
using canvas::SceneOrderKey;
using canvas::SceneRecord;
using canvas::SceneRecordFlags;
using canvas::SceneRevision;
using canvas::UniformGridSpatialIndex;
using canvas::WorldPoint;
using canvas::WorldRect;
using canvas::render::CameraGeneration;
using canvas::render::CameraState;
using canvas::render::SurfaceMetrics;

class CanonicalFixtureStore final : public canvas::semantic::ObjectStore {
  public:
    explicit CanonicalFixtureStore(canvas::semantic::ObjectRecord record)
        : records_{std::move(record)} {}

    std::size_t size() const noexcept override { return records_.size(); }
    bool contains(const canvas::semantic::ObjectId& id) const noexcept override {
        return find(id) != nullptr;
    }
    const canvas::semantic::ObjectRecord* find(
        const canvas::semantic::ObjectId& id) const noexcept override {
        for (const auto& value : records_) {
            if (value.id == id) return &value;
        }
        return nullptr;
    }
    std::vector<canvas::semantic::ObjectRecord> allObjects() const override { return records_; }
    std::vector<canvas::semantic::ObjectRecord> children(
        const std::optional<canvas::semantic::ObjectId>&) const override {
        return {};
    }

  private:
    std::vector<canvas::semantic::ObjectRecord> records_;
};

canvas::semantic::ObjectRecord canonicalShape() {
    canvas::semantic::ObjectRecord value;
    value.id = canvas::semantic::ObjectId::fromUint64(202);
    value.kind = canvas::semantic::ObjectKind::kShape;
    value.kind_version = 1U;
    value.placement.order_key = canvas::semantic::OrderKey({2U});
    value.transform.tx = 1.0;
    value.content = canvas::semantic::ShapeContent{1U, 20.0, 20.0};
    return value;
}

SceneRecord record(std::uint64_t id, std::uint64_t order) {
    return SceneRecord{
        .objectId = ObjectId::fromUint64(id),
        .orderKey = SceneOrderKey(order),
        .kind = SceneObjectKind::kShape,
        .flags = static_cast<SceneRecordFlags>(
            static_cast<std::uint32_t>(SceneRecordFlags::kVisible) |
            static_cast<std::uint32_t>(SceneRecordFlags::kHitTestable)),
        .worldBounds = WorldRect{0.0F, 0.0F, 20.0F, 20.0F},
        .contentRevision = ContentRevision{1},
        .renderPayload = RenderPayloadRef{static_cast<std::uint32_t>(id), 1},
        .hitGeometry = HitGeometryRef{static_cast<std::uint32_t>(id), 1},
    };
}

void delegatesToG2AndKeepsSelectionTransient() {
    Scene scene(std::make_unique<DirectRenderScene>(),
                std::make_unique<UniformGridSpatialIndex>());
    assert(scene.replace(CompiledSceneSnapshot{
        .sourceRevision = SceneRevision{9},
        .records = {record(101, 1), record(202, 2)},
    }));
    CanonicalFixtureStore canonical(canonicalShape());
    const auto canonicalBefore = canonical.allObjects();
    RuntimeScene runtime;
    assert(runtime.replace(canvas::semantic::SemanticReadView(
        canonical, canvas::semantic::SemanticGeneration{17})));
    const auto runtimeBefore = std::vector<canvas::RuntimeSceneRecord>(
        runtime.records().begin(), runtime.records().end());
    const auto runtimeGenerationBefore = runtime.generation();
    const auto canonicalGenerationBefore = canvas::semantic::SemanticGeneration{17};
    const auto sceneBefore = scene.read().records();
    const auto revisionBefore = scene.revision();

    SceneHitTestPort port(scene);
    DemoHitSelectHarness harness(port);
    const CameraController camera{
        CameraState{WorldPoint{10.0F, 10.0F}, 1.0F, 0.0F, CameraGeneration{3}}};
    const SurfaceMetrics metrics{200.0F, 200.0F, 200, 200, 1.0F, 1.0F};
    const auto selected = harness.selectAtViewPoint(WorldPoint{100.0F, 100.0F},
                                                    camera,
                                                    metrics);
    assert(selected.hasValue());
    assert(selected.value().hit.frontToBack.front() == ObjectId::fromUint64(202));
    assert(selected.value().hit.diagnostics.candidatesExamined > 0);
    assert(selected.value().hit.diagnostics.candidatesAfterFilter == 2);
    assert(selected.value().hit.diagnostics.preciseTests > 0);
    assert(harness.transient().selectedObject == ObjectId::fromUint64(202));
    assert(harness.transient().selectionGeneration == 1);
    assert(!harness.transient().overlayDigest.empty());

    assert(scene.revision() == revisionBefore);
    assert(scene.read().records().size() == sceneBefore.size());
    for (std::size_t index = 0; index < sceneBefore.size(); ++index) {
        assert(scene.read().records()[index] == sceneBefore[index]);
    }
    assert(canonical.allObjects() == canonicalBefore);
    assert(runtime.generation() == runtimeGenerationBefore);
    assert(std::vector<canvas::RuntimeSceneRecord>(runtime.records().begin(), runtime.records().end()) ==
           runtimeBefore);
    assert(canonicalGenerationBefore == canvas::semantic::SemanticGeneration{17});

    const auto cleared = harness.selectAtViewPoint(WorldPoint{180.0F, 180.0F},
                                                   camera,
                                                   metrics);
    assert(cleared.hasValue());
    assert(cleared.value().hit.frontToBack.empty());
    assert(!harness.transient().selectedObject.has_value());
    assert(harness.transient().selectionGeneration == 2);
}

} // namespace

int main() {
    delegatesToG2AndKeepsSelectionTransient();
    return 0;
}
