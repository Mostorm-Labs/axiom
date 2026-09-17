#include "canvas/render/visibility_resolver.hpp"

#include "canvas/scene/direct_render_scene.hpp"
#include "canvas/scene/scene.hpp"
#include "canvas/scene/uniform_grid_spatial_index.hpp"

#include <cassert>
#include <cstdint>
#include <iostream>
#include <memory>
#include <vector>

namespace {

using canvas::CompiledSceneSnapshot;
using canvas::ContentRevision;
using canvas::DirectRenderScene;
using canvas::ObjectId;
using canvas::RenderPayloadRef;
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
using canvas::render::FrameId;
using canvas::render::FrameState;
using canvas::render::MetricsGeneration;
using canvas::render::SurfaceGeneration;
using canvas::render::SurfaceMetrics;
using canvas::render::ViewId;
using canvas::render::VisibilityResolver;
using canvas::semantic::SemanticGeneration;

SceneRecord record(std::uint64_t id, std::uint64_t order, WorldRect bounds) {
    return SceneRecord{
        .objectId = ObjectId::fromUint64(id),
        .orderKey = SceneOrderKey(order),
        .kind = SceneObjectKind::kShape,
        .flags = SceneRecordFlags::kVisible,
        .worldBounds = bounds,
        .contentRevision = ContentRevision{1},
        .renderPayload = RenderPayloadRef{static_cast<std::uint32_t>(id), 1},
        .hitGeometry = {static_cast<std::uint32_t>(id), 1},
    };
}

FrameState frame(WorldRect viewport, std::uint64_t cameraGeneration) {
    return FrameState{
        .viewId = ViewId{7},
        .camera = CameraState{WorldPoint{}, 1.0F, 0.0F,
                              CameraGeneration{cameraGeneration}},
        .worldViewport = viewport,
        .metrics = SurfaceMetrics{800, 600, 1600, 1200, 2.0F, 1.0F},
        .sceneGeneration = SemanticGeneration{9},
        .sceneReadToken = SceneRevision{9},
        .surfaceGeneration = SurfaceGeneration{3},
        .metricsGeneration = MetricsGeneration{4},
        .frameId = FrameId{cameraGeneration},
    };
}

std::unique_ptr<Scene> makeScene(std::vector<SceneRecord> records) {
    auto scene = std::make_unique<Scene>(std::make_unique<DirectRenderScene>(),
                                         std::make_unique<UniformGridSpatialIndex>(10.0F));
    auto replaced = scene->replace(CompiledSceneSnapshot{SceneRevision{9}, std::move(records)});
    assert(replaced.hasValue());
    return scene;
}

void renderOwnsViewportQueryAndFinalOrder() {
    auto scene = makeScene({
        record(1, 30, WorldRect{0, 0, 4, 4}),
        record(2, 10, WorldRect{1, 1, 5, 5}),
        record(3, 20, WorldRect{2, 2, 6, 6}),
        record(4, 40, WorldRect{100, 100, 104, 104}),
    });

    const auto result = VisibilityResolver::resolve(frame(WorldRect{-1, -1, 8, 8}, 1), *scene);
    assert(result.hasValue());
    assert(result.value().sceneGeneration == SemanticGeneration{9});
    assert(result.value().sceneReadToken == SceneRevision{9});
    assert(result.value().queryWorldRect == (WorldRect{-1, -1, 8, 8}));
    assert((result.value().backToFront ==
            std::vector<ObjectId>{ObjectId::fromUint64(2), ObjectId::fromUint64(3),
                                  ObjectId::fromUint64(1)}));
    assert(result.value().visibleRecords == 3);
    assert(result.value().candidatesExamined >= result.value().visibleRecords);
}

void cameraOnlyChangeLeavesCanonicalSceneUnchanged() {
    const std::vector<SceneRecord> source{
        record(11, 1, WorldRect{0, 0, 4, 4}),
        record(12, 2, WorldRect{100, 100, 104, 104}),
    };
    auto scene = makeScene(source);
    canvas::RuntimeScene runtimeScene;
    const auto runtimeGenerationBefore = runtimeScene.generation();
    const auto runtimeRecordsBefore = std::vector<canvas::RuntimeSceneRecord>{
        runtimeScene.records().begin(), runtimeScene.records().end()};
    const auto beforeRecords = std::vector<SceneRecord>{scene->read().records().begin(),
                                                         scene->read().records().end()};

    const auto first = VisibilityResolver::resolve(frame(WorldRect{-1, -1, 8, 8}, 1), *scene);
    const auto second = VisibilityResolver::resolve(frame(WorldRect{99, 99, 108, 108}, 2), *scene);
    assert(first.hasValue() && second.hasValue());
    assert(first.value().backToFront == std::vector<ObjectId>{ObjectId::fromUint64(11)});
    assert(second.value().backToFront == std::vector<ObjectId>{ObjectId::fromUint64(12)});
    assert(scene->semanticGeneration() == SemanticGeneration{9});
    assert(scene->revision() == SceneRevision{9});
    assert(std::vector<SceneRecord>(scene->read().records().begin(), scene->read().records().end()) ==
           beforeRecords);
    assert(runtimeScene.generation() == runtimeGenerationBefore);
    const auto runtimeRecordsAfter =
        std::vector<canvas::RuntimeSceneRecord>{runtimeScene.records().begin(),
                                                runtimeScene.records().end()};
    assert(runtimeRecordsAfter == runtimeRecordsBefore);
}

void localizedResolutionDoesNotScaleWithTotalSceneSize() {
    std::vector<SceneRecord> records;
    records.reserve(100000);
    for (std::uint64_t id = 1; id <= 100000; ++id) {
        const float x = id <= 500 ? static_cast<float>(id % 50U) : 10000.0F + static_cast<float>(id);
        records.push_back(record(id, id, WorldRect{x, 0.0F, x + 1.0F, 1.0F}));
    }
    auto scene = makeScene(std::move(records));
    const auto result = VisibilityResolver::resolve(frame(WorldRect{0, -1, 50, 2}, 3), *scene);
    assert(result.hasValue());
    assert(result.value().visibleRecords == 500);
    assert(result.value().candidatesExamined < 5000);
    std::cout << "{\"total_scene_records\":100000,\"visible_records\":"
              << result.value().visibleRecords << ",\"candidates_examined\":"
              << result.value().candidatesExamined
              << ",\"semantic_object_iterations\":0}\n";
}

} // namespace

int main() {
    renderOwnsViewportQueryAndFinalOrder();
    cameraOnlyChangeLeavesCanonicalSceneUnchanged();
    localizedResolutionDoesNotScaleWithTotalSceneSize();
    return 0;
}
