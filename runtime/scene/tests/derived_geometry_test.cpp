#include "canvas/scene/direct_render_scene.hpp"
#include "canvas/scene/derived_geometry.hpp"
#include "canvas/semantic/object_content.hpp"
#include "canvas/scene/scene.hpp"
#include "canvas/scene/uniform_grid_spatial_index.hpp"
#include <cstdlib>
#include <iostream>
#include <memory>
#include <algorithm>
#include <array>
#include <cmath>

// SH2-G01/G02/G04/G13/G14: independent scanline containment and affine
// projection, compared with the real semantic-to-derived-geometry/Scene path.
bool frozen_triangle_geometry_and_transforms() {
    using namespace canvas;
    semantic::ObjectRecord object{};
    object.id = ObjectId::fromUint64(42);
    object.kind = semantic::ObjectKind::kShape;
    object.kind_version = 2;
    object.content = semantic::ShapeContent{3, 100, 80};
    const scene::TriangleGeometry expected{{50,0},{100,80},{0,80},{0,0,100,80}};
    const auto geometry = scene::deriveGeometry(object);
    if (!std::holds_alternative<scene::TriangleGeometry>(geometry) ||
        std::get<scene::TriangleGeometry>(geometry) != expected) return false;
    auto resized = object;
    resized.content = semantic::ShapeContent{3, 200, 40};
    const scene::TriangleGeometry expectedResize{{100,0},{200,40},{0,40},{0,0,200,40}};
    if (scene::deriveGeometry(resized) != scene::DerivedGeometry{expectedResize}) return false;
    object.properties.entries = {{0x100U, semantic::FillStyleValue{semantic::NoFill{}}},
                                 {0x101U, semantic::StrokeStyleValue{semantic::NoStroke{}}}};
    if (scene::deriveGeometry(object) != geometry) return false;
    const std::array<semantic::Transform2D, 4> transforms{{{}, {1,0,0,1,30,20},
                                                       {0,1,-1,0,150,20}, {-1,0,0,1,150,20}}};
    std::size_t probes = 0;
    for (const auto& transform : transforms) {
        const auto world = [&](float x, float y) {
            return WorldPoint{static_cast<float>(transform.a*x + transform.c*y + transform.tx),
                              static_cast<float>(transform.b*x + transform.d*y + transform.ty)};
        };
        const auto a = world(0,0), b = world(100,80);
        WorldRect bounds{std::min(a.x,b.x),std::min(a.y,b.y),std::max(a.x,b.x),std::max(a.y,b.y)};
        SceneRecord record{object.id, SceneOrderKey(1), SceneObjectKind::kShape,
            static_cast<SceneRecordFlags>(5), bounds, ContentRevision(1), {1,1}, {1,1}, transform, geometry};
        record.properties.entries = {{0x100U, semantic::FillStyleValue{semantic::SolidFill{{1,0,0,1}}}},
                                     {0x101U, semantic::StrokeStyleValue{semantic::NoStroke{}}}};
        Scene scene(std::make_unique<DirectRenderScene>(), std::make_unique<UniformGridSpatialIndex>());
        if (!scene.replace({SceneRevision(1),{record}})) return false;
        for (int y=-10; y<=90; y+=5) for (int x=-10; x<=110; x+=5) {
            // Independent horizontal half-width of the frozen isosceles shape.
            const bool inside = y>=0 && y<=80 && std::abs(x-50)*80 <= y*50;
            const auto result = scene.hitTest({world(static_cast<float>(x),static_cast<float>(y)),0,{},1});
            if (!result || (!result.value().frontToBack.empty()) != inside) {
                std::cerr << "SH2 transformed hit divergence at local " << x << ',' << y << '\n';
                return false;
            }
            ++probes;
        }
    }
    std::cout << "PASS SH2 frozen geometry, resized/None and affine hit probes: " << probes << '\n';
    return true;
}

int main() {
    if (!frozen_triangle_geometry_and_transforms()) return EXIT_FAILURE;
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
