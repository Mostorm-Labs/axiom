#include "canvas/scene/sketch_geometry.hpp"
#include <gtest/gtest.h>
using namespace canvas;
namespace {
semantic::PropertyBag style(double rough = 1) {
    return {{{0x102U, semantic::SketchStyleV1{1, 99, rough, true}}}};
}
} // namespace
TEST(SketchGeometry, DeterministicAndColorIndependent) {
    semantic::ObjectContent shape = semantic::ShapeContent{1, 120, 80};
    auto p = style();
    auto a = scene::sketchGeometry(shape, p);
    ASSERT_TRUE(a);
    ASSERT_EQ(a->outline.commands.size(), 16);
    auto b = scene::sketchGeometry(shape, p);
    EXPECT_EQ(a, b);
    p.entries.insert(p.entries.begin(), {3U, .2F});
    auto c = scene::sketchGeometry(shape, p);
    EXPECT_EQ(a, c);
    EXPECT_LT(a->bounds.top, 0);
    EXPECT_GE(a->bounds.right, 120);
}
TEST(SketchGeometry, RoughnessZeroExactAndContinuous) {
    semantic::ObjectContent shape = semantic::ShapeContent{1, 120, 80};
    auto a = scene::sketchGeometry(shape, style(0));
    ASSERT_TRUE(a);
    EXPECT_EQ(a->outline.commands.size(), 5);
    EXPECT_EQ(a->bounds, (foundation::WorldRect{0, 0, 120, 80}));
    auto b = scene::sketchGeometry(shape, style(1));
    auto c = scene::sketchGeometry(shape, style(2));
    const auto &x = std::get<semantic::CubicTo>(b->outline.commands[1]);
    const auto &y = std::get<semantic::CubicTo>(c->outline.commands[1]);
    EXPECT_NEAR(y.control1.y, 2 * x.control1.y, 1e-6);
}
TEST(SketchGeometry, HatchBoundedIndependentAndCanCoexistWithRegularOutline) {
    semantic::ObjectContent shape = semantic::ShapeContent{2, 16000, 16000};
    auto p = style(0);
    p.entries.insert(
        p.entries.begin(),
        {0x100U, semantic::FillStyleValue{semantic::PatternFillV1{
                     1, {1, 0, 0, 1}, semantic::PatternKind::kCrosshatch, -45, 2, 1}}});
    auto a = scene::sketchGeometry(shape, p);
    ASSERT_TRUE(a);
    EXPECT_GT(a->hatches.commands.size(), 1000);
    EXPECT_LE(a->hatches.commands.size(), 32768);
    std::get<semantic::PatternFillV1>(std::get<semantic::FillStyleValue>(p.entries[0].value))
        .color = {0, 1, 0, 1};
    auto b = scene::sketchGeometry(shape, p);
    EXPECT_EQ(a, b);
}
TEST(SketchGeometry, ConnectorKeepsEndpointsAndDirections) {
    scene::ConnectorGeometry route;
    route.start = {10, 20};
    route.end = {210, 120};
    route.route = {{10, 20}, {100, 20}, {100, 120}, {210, 120}};
    semantic::ObjectContent c = semantic::ConnectorContent{};
    auto a = scene::sketchGeometry(c, style(3), &route);
    ASSERT_TRUE(a);
    const auto &first = std::get<semantic::MoveTo>(a->outline.commands.front());
    const auto &bez = std::get<semantic::CubicTo>(a->outline.commands[1]);
    EXPECT_EQ(first.point, (semantic::Vec2{10, 20}));
    EXPECT_EQ(bez.control1.y, 20);
    const auto &last = std::get<semantic::CubicTo>(a->outline.commands.back());
    EXPECT_EQ(last.end, (semantic::Vec2{210, 120}));
    EXPECT_EQ(last.control2.y, 120);
}
TEST(SketchGeometry, HatchCurvesUseIndependentSeededStream) {
    semantic::ObjectContent shape = semantic::ShapeContent{1, 120, 80};
    auto p = style(2);
    p.entries.insert(p.entries.begin(),
                     {0x100U, semantic::FillStyleValue{semantic::PatternFillV1{
                                  1, {1, 0, 0, 1}, semantic::PatternKind::kDiagonal, -45, 8, 1}}});
    auto a = scene::sketchGeometry(shape, p);
    ASSERT_TRUE(a);
    ASSERT_GT(a->hatches.commands.size(), 2);
    EXPECT_TRUE(std::holds_alternative<semantic::CubicTo>(a->hatches.commands[1]));
    std::get<semantic::PatternFillV1>(std::get<semantic::FillStyleValue>(p.entries[0].value)).gap =
        4;
    auto b = scene::sketchGeometry(shape, p);
    EXPECT_EQ(a->outline, b->outline);
}
TEST(SketchGeometry, SingleConnectorSegmentHasVisibleBowWithExactTerminalTangents) {
    scene::ConnectorGeometry route;
    route.start = {0, 0};
    route.end = {120, 0};
    route.route = {{0, 0}, {120, 0}};
    const auto path = scene::sketchGeometry(semantic::ObjectContent{semantic::ConnectorContent{}},
                                            style(3), &route);
    ASSERT_TRUE(path);
    bool bow = false;
    for (const auto &cmd : path->outline.commands)
        if (const auto *c = std::get_if<semantic::CubicTo>(&cmd))
            bow = bow || c->control1.y != 0 || c->control2.y != 0;
    EXPECT_TRUE(bow);
}
TEST(SketchGeometry, ObstacleHullFallbackCannotBowIntoAdmittedObstacle) {
    scene::ConnectorGeometry route;
    route.start = {0, 0};
    route.end = {120, 0};
    route.route = {{0, 0}, {120, 0}};
    route.sketchObstacles = {{0, -40, 120, 0}};
    const auto path = scene::sketchGeometry(semantic::ObjectContent{semantic::ConnectorContent{}},
                                            style(3), &route);
    ASSERT_TRUE(path);
    for (const auto &cmd : path->outline.commands)
        if (const auto *c = std::get_if<semantic::CubicTo>(&cmd)) {
            EXPECT_GE(c->control1.y, 0);
            EXPECT_GE(c->control2.y, 0);
            EXPECT_GE(c->end.y, 0);
        }
}
#include "canvas/scene/bounds_system.hpp"
TEST(SketchGeometry, VisualBoundsCoverBezierHullsWhileGeometryBoundsRemainStable) {
    for (unsigned kind = 1; kind <= 3; ++kind) {
        semantic::ObjectRecord object;
        object.kind = semantic::ObjectKind::kShape;
        object.kind_version = kind == 3 ? 2 : 1;
        object.content = semantic::ShapeContent{kind, 120, 80};
        object.properties = style(3);
        object.properties.entries.insert(
            object.properties.entries.begin(),
            {0x101U, semantic::StrokeStyleValue{semantic::SolidStroke{{0, 0, 0, 1},
                                                                      12,
                                                                      semantic::StrokeCap::kSquare,
                                                                      semantic::MiterJoin{4},
                                                                      semantic::SolidDash{}}}});
        const auto bounds = scene::computeBounds(object);
        EXPECT_EQ(bounds.geometry, (foundation::WorldRect{0, 0, 120, 80}));
        const auto paths = scene::sketchGeometry(object.content, object.properties);
        ASSERT_TRUE(paths);
        const auto inside = [&](semantic::Vec2 p) {
            EXPECT_LE(bounds.visual.left, p.x - 6);
            EXPECT_LE(bounds.visual.top, p.y - 6);
            EXPECT_GE(bounds.visual.right, p.x + 6);
            EXPECT_GE(bounds.visual.bottom, p.y + 6);
        };
        for (const auto &c : paths->outline.commands)
            std::visit(
                [&](const auto &v) {
                    using T = std::decay_t<decltype(v)>;
                    if constexpr (std::is_same_v<T, semantic::MoveTo>)
                        inside(v.point);
                    else if constexpr (std::is_same_v<T, semantic::CubicTo>) {
                        inside(v.control1);
                        inside(v.control2);
                        inside(v.end);
                    }
                },
                c);
    }
}
