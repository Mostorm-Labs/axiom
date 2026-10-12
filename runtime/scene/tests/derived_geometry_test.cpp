#include "canvas/scene/direct_render_scene.hpp"
#include "canvas/scene/derived_geometry.hpp"
#include "canvas/semantic/object_content.hpp"
#include "canvas/scene/scene.hpp"
#include "canvas/scene/uniform_grid_spatial_index.hpp"
#include <cstdlib>
#include <iostream>
#include <memory>

int main() {
    canvas::Scene scene(std::make_unique<canvas::DirectRenderScene>(), std::make_unique<canvas::UniformGridSpatialIndex>());
    canvas::SceneRecord record{canvas::ObjectId::fromUint64(1),canvas::SceneOrderKey(1),canvas::SceneObjectKind::kShape,
        static_cast<canvas::SceneRecordFlags>(5),{0,0,100,80},canvas::ContentRevision(1),{1,1},{1,1},canvas::semantic::Transform2D{},canvas::scene::DerivedGeometry{canvas::scene::TriangleGeometry{{50,0},{100,80},{0,80},{0,0,100,80}}}};
    const auto replace = scene.replace({canvas::SceneRevision(1),{record}});
    if (!replace) return EXIT_FAILURE;
    // Independent triangle outside probe within its bbox. Baseline RF01 only
    // has bbox precision, so this deliberately exposes the missing geometry.
    const auto outside = scene.hitTest({{5,10},0,{},1});
    if (!outside || !outside.value().frontToBack.empty()) {
        std::cerr << "triangle exterior inside bbox must not hit\n";
        return EXIT_FAILURE;
    }
    canvas::Scene transformed(std::make_unique<canvas::DirectRenderScene>(), std::make_unique<canvas::UniformGridSpatialIndex>());
    auto transformedRecord = record;
    transformedRecord.localTransform = canvas::semantic::Transform2D{2.0, 0.0, 0.0, 2.0, 100.0, 0.0};
    transformedRecord.worldBounds = {100.0F, 0.0F, 300.0F, 160.0F};
    const auto transformedReplace = transformed.replace({canvas::SceneRevision(1), {transformedRecord}});
    if (!transformedReplace) return EXIT_FAILURE;
    const auto transformedInside = transformed.hitTest({{200.0F, 120.0F}, 0.0F, {}, 1});
    if (!transformedInside || transformedInside.value().frontToBack.size() != 1U) {
        std::cerr << "transformed triangle must hit in world coordinates\n";
        return EXIT_FAILURE;
    }
    canvas::Scene noFill(std::make_unique<canvas::DirectRenderScene>(), std::make_unique<canvas::UniformGridSpatialIndex>());
    auto outline = record;
    outline.properties.entries = {{0x100U, canvas::semantic::FillStyleValue{canvas::semantic::NoFill{}}},
                                 {0x101U, canvas::semantic::StrokeStyleValue{canvas::semantic::SolidStroke{{0,0,0,1}, 2.0}}}};
    if (!noFill.replace({canvas::SceneRevision(1), {outline}})) return EXIT_FAILURE;
    const auto interior = noFill.hitTest({{50.0F, 40.0F}, 0.0F, {}, 1});
    if (!interior || !interior.value().frontToBack.empty()) {
        std::cerr << "None fill triangle interior must not hit\n";
        return EXIT_FAILURE;
    }
    canvas::Scene arrow(std::make_unique<canvas::DirectRenderScene>(), std::make_unique<canvas::UniformGridSpatialIndex>());
    auto arrowRecord = record;
    arrowRecord.kind = canvas::SceneObjectKind::kConnector;
    arrowRecord.worldBounds = {0.0F, -8.0F, 100.0F, 8.0F};
    arrowRecord.derivedGeometry = canvas::scene::ConnectorGeometry{{0.0F,0.0F},{100.0F,0.0F},true};
    if (!arrow.replace({canvas::SceneRevision(1), {arrowRecord}})) return EXIT_FAILURE;
    const auto arrowInterior = arrow.hitTest({{90.0F, 4.0F}, 0.0F, {}, 1});
    if (!arrowInterior || arrowInterior.value().frontToBack.size() != 1U) {
        std::cerr << "arrowhead interior off line segment must hit\n";
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
