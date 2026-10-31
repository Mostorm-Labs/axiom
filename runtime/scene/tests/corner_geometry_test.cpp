#include "canvas/render/selection_region_geometry.hpp"
#include "canvas/scene/corner_geometry.hpp"
#include <algorithm>
#include <gtest/gtest.h>
using namespace canvas;
TEST(CornerGeometry, RectangleAndTriangleUseCurvesAndClampExcessRadius) {
    for (const auto &points : {std::vector<semantic::Vec2>{{0, 0}, {120, 0}, {120, 80}, {0, 80}},
                               std::vector<semantic::Vec2>{{60, 0}, {120, 80}, {0, 80}}}) {
        const auto p = scene::roundedPolyline(points, true, 16);
        EXPECT_EQ(std::count_if(
                      p.commands.begin(), p.commands.end(),
                      [](const auto &c) { return std::holds_alternative<semantic::CubicTo>(c); }),
                  points.size());
        EXPECT_EQ(scene::roundedPolyline(points, true, 16384),
                  scene::roundedPolyline(points, true, 32768));
        EXPECT_EQ(scene::roundedPolyline(points, true, 0).commands.size(), points.size() + 1);
    }
}
TEST(CornerGeometry, OpenRoutePreservesEndpointsAndAvoidsBlockedCorner) {
    const std::vector<semantic::Vec2> points{{0, 0}, {100, 0}, {100, 100}};
    const auto p = scene::roundedPolyline(points, false, 20);
    ASSERT_EQ(p.commands.size(), 4);
    EXPECT_EQ(std::get<semantic::MoveTo>(p.commands.front()).point, points.front());
    EXPECT_EQ(std::get<semantic::LineTo>(p.commands.back()).end, points.back());
    EXPECT_TRUE(std::holds_alternative<semantic::CubicTo>(p.commands[2]));
    const std::array<foundation::WorldRect, 1> obstacles{{{81, 1, 99, 19}}};
    const auto safe = scene::roundedPolyline(points, false, 20, obstacles);
    EXPECT_NE(p, safe);
    EXPECT_EQ(std::get<semantic::LineTo>(safe.commands.back()).end, points.back());
}
TEST(CornerGeometry, RoundedTriangleAttachmentLiesOnRoundedBoundary) {
    semantic::Vec2 point, normal;
    ASSERT_TRUE(scene::roundedShapePerimeter({3, 120, 80}, 16, {0, -1}, point, normal));
    EXPECT_NEAR(point.x, 60, 1e-6);
    EXPECT_GT(point.y, 0);
    EXPECT_LT(normal.y, 0);
}

TEST(CornerGeometry, ExistingSketchEllipsesRemainAcquirableWithoutRoundedProperty) {
    const std::array<foundation::WorldPoint, 4> region{
        {{-10, -10}, {200, -10}, {200, 150}, {-10, 150}}};
    for (const double roughness : {0., 1., 3.})
        for (std::uint64_t seed = 1; seed <= 32; ++seed) {
            SCOPED_TRACE(seed);
            SCOPED_TRACE(roughness);
            RuntimeSceneRecord record;
            record.kind = semantic::ObjectKind::kShape;
            record.kindVersion = 1;
            record.content = semantic::ShapeContent{2, 140, 100};
            record.properties.entries = {
                {0x100U, semantic::FillStyleValue{semantic::SolidFill{{.2F, .5F, .8F, 1}}}},
                {0x101U, semantic::StrokeStyleValue{semantic::SolidStroke{{0, 0, 0, 1}, 1}}},
                {0x102U, semantic::SketchStyleV1{1, seed, roughness, true}}};
            EXPECT_EQ(render::intersectVisibleSelectionGeometry(record, region),
                      render::SelectionGeometryHit::kHit);
        }
}

TEST(CornerGeometry, FullyErasedBodyAndDerivedOutlineDoNotRemainSelectable) {
    const std::array<foundation::WorldPoint, 4> region{
        {{-30, -30}, {170, -30}, {170, 130}, {-30, 130}}};
    for (const double radius : {0., 20.}) {
        RuntimeSceneRecord record;
        record.kind = semantic::ObjectKind::kShape;
        record.kindVersion = 1;
        record.content = semantic::ShapeContent{1, 140, 100};
        record.properties.entries = {
            {0x100U, semantic::FillStyleValue{semantic::SolidFill{{.2F, .5F, .8F, 1}}}},
            {0x101U, semantic::StrokeStyleValue{semantic::SolidStroke{{0, 0, 0, 1}, 2}}},
            {0x102U, semantic::SketchStyleV1{1, 11, 2, true}}};
        if (radius)
            record.properties.entries.push_back({0x103U, semantic::CornerStyleV1{1, radius}});
        ASSERT_EQ(render::intersectVisibleSelectionGeometry(record, region),
                  render::SelectionGeometryHit::kHit);
        semantic::VectorPathGeometry mask;
        mask.commands = {semantic::MoveTo{{-40, -40}}, semantic::LineTo{{180, -40}},
                         semantic::LineTo{{180, 140}}, semantic::LineTo{{-40, 140}},
                         semantic::ClosePath{}};
        record.eraseMasks = {{foundation::ObjectId::fromUint64(1), semantic::FilledPathMask{mask}}};
        EXPECT_EQ(render::intersectVisibleSelectionGeometry(record, region),
                  render::SelectionGeometryHit::kMiss);
    }
}
