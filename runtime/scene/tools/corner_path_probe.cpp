#include "canvas/scene/sketch_geometry.hpp"
#include <iomanip>
#include <iostream>
using namespace canvas;
int main() {
    std::cout << std::setprecision(17) << '[';
    bool first = true;
    const auto emit = [&](const semantic::VectorPathGeometry &p) {
        for (const auto &c : p.commands) {
            if (!first)
                std::cout << ',';
            first = false;
            std::cout << '[' << c.index();
            const auto point = [](semantic::Vec2 p) { std::cout << ',' << p.x << ',' << p.y; };
            std::visit(
                [&](const auto &v) {
                    using T = std::decay_t<decltype(v)>;
                    if constexpr (std::is_same_v<T, semantic::MoveTo>)
                        point(v.point);
                    else if constexpr (!std::is_same_v<T, semantic::ClosePath>) {
                        if constexpr (std::is_same_v<T, semantic::QuadTo>)
                            point(v.control);
                        if constexpr (std::is_same_v<T, semantic::CubicTo>) {
                            point(v.control1);
                            point(v.control2);
                        }
                        point(v.end);
                    }
                },
                c);
            std::cout << ']';
        }
    };
    for (double size : {6., 120., 16000.})
        for (double radius : {0., 2., 16., 24., 1000.})
            for (double rough : {0., 1., 3.})
                for (bool dash : {false, true}) {
                    semantic::SolidStroke stroke{{0, 0, 0, 1}, 3};
                    if (dash)
                        stroke.dash = semantic::DashPattern{{8, 5}, 0};
                    semantic::PropertyBag props{
                        {{0x100U, semantic::FillStyleValue{semantic::PatternFillV1{
                                      1,
                                      {.2F, .5F, .8F, 1},
                                      semantic::PatternKind::kCrosshatch,
                                      -33,
                                      8,
                                      1}}},
                         {0x101U, semantic::StrokeStyleValue{stroke}},
                         {0x102U, semantic::SketchStyleV1{1, 9223372036854775809ULL, rough, true}},
                         {0x103U, semantic::CornerStyleV1{1, radius}}}};
                    for (unsigned kind : {1U, 3U}) {
                        const auto g = scene::sketchGeometry(
                            semantic::ShapeContent{kind, size, size * .7}, props);
                        if (!g)
                            return 1;
                        emit(g->body);
                        emit(g->outline);
                        emit(g->hatches);
                    }
                    props.entries.erase(props.entries.begin());
                    props.entries.push_back({0x201U, semantic::ConnectorDecorationValue::kArrow});
                    scene::ConnectorGeometry route;
                    route.start = {0, 0};
                    route.end = {260, 80};
                    route.route = {{0, 0}, {120, 0}, {120, 80}, {260, 80}};
                    route.status = runtime::ConnectorRouteStatus::kReady;
                    route.sketchObstacles = {{80, 1, 118, 35}};
                    const auto g =
                        scene::sketchGeometry(semantic::ConnectorContent{}, props, &route);
                    if (!g)
                        return 2;
                    emit(g->body);
                    emit(g->outline);
                    emit(g->arrowFill);
                }
    std::cout << "]\n";
}
