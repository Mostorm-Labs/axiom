#include "canvas/scene/sketch_geometry.hpp"
#include <iomanip>
#include <iostream>
using namespace canvas;
int main(int argc, char **) {
    std::cout << std::setprecision(17) << '[';
    bool first = true;
    if (argc > 1) {
        const auto emit = [&](const semantic::VectorPathGeometry &path) {
            for (const auto &c : path.commands) {
                if (!first)
                    std::cout << ',';
                first = false;
                std::cout << '[' << c.index();
                const auto p = [](semantic::Vec2 v) { std::cout << ',' << v.x << ',' << v.y; };
                std::visit(
                    [&](const auto &v) {
                        using T = std::decay_t<decltype(v)>;
                        if constexpr (std::is_same_v<T, semantic::MoveTo>)
                            p(v.point);
                        else if constexpr (!std::is_same_v<T, semantic::ClosePath>) {
                            if constexpr (std::is_same_v<T, semantic::QuadTo>)
                                p(v.control);
                            if constexpr (std::is_same_v<T, semantic::CubicTo>) {
                                p(v.control1);
                                p(v.control2);
                            }
                            p(v.end);
                        }
                    },
                    c);
                std::cout << ']';
            }
        };
        for (double rough : {0., .01, 1., 2., 3.})
            for (double size : {6., 120., 16000.})
                for (unsigned dash = 0; dash < 2; ++dash) {
                    semantic::VectorPathContent path{
                        {semantic::FillRule::kEvenOdd,
                         {semantic::MoveTo{{0, 0}},
                          semantic::QuadTo{{size / 2, -size / 3}, {size, 0}},
                          semantic::CubicTo{{size, size}, {0, size}, {0, 0}}, semantic::ClosePath{},
                          semantic::MoveTo{{size / 3, size / 4}},
                          semantic::LineTo{{size * .6, size / 4}},
                          semantic::LineTo{{size / 2, size * .6}}, semantic::ClosePath{}}}};
                    semantic::TableContent table;
                    table.rows = {{{foundation::ObjectId::fromUint64(11)}, size / 3},
                                  {{foundation::ObjectId::fromUint64(12)}, size / 2}};
                    table.columns = {{{foundation::ObjectId::fromUint64(21)}, size / 2},
                                     {{foundation::ObjectId::fromUint64(22)}, size / 3}};
                    table.cells.resize(4);
                    semantic::SolidStroke stroke{{0, 0, 0, 1}, 3};
                    if (dash)
                        stroke.dash = semantic::DashPattern{{8, 5}, 0};
                    semantic::PropertyBag props{
                        {{0x100U,
                          semantic::FillStyleValue{semantic::PatternFillV1{
                              1, {1, 0, 0, 1}, semantic::PatternKind::kCrosshatch, -33, 8, 1}}},
                         {0x101U, semantic::StrokeStyleValue{stroke}},
                         {0x102U,
                          semantic::SketchStyleV1{1, 9223372036854775809ULL, rough, true}}}};
                    for (const auto &content :
                         {semantic::ObjectContent{path}, semantic::ObjectContent{table},
                          semantic::ObjectContent{
                              semantic::SectionContent{{10, 20, size, size * .7}, "", 6, 24}}}) {
                        const auto g = scene::sketchGeometry(content, props);
                        if (!g)
                            return 1;
                        emit(g->body);
                        emit(g->outline);
                        emit(g->hatches);
                    }
                }
        std::cout << "]\n";
        return 0;
    }
    for (unsigned kind = 1; kind <= 3; ++kind)
        for (double size : {6., 120., 16000.})
            for (double rough : {0., .01, 1., 2., 3.})
                for (unsigned dash = 0; dash < 2; ++dash) {
                    semantic::ObjectContent shape = semantic::ShapeContent{kind, size, size * .7};
                    semantic::SolidStroke stroke{{0, 0, 0, 1}, 2};
                    if (dash)
                        stroke.dash = semantic::DashPattern{{8, 5}, 0};
                    semantic::PropertyBag props{
                        {{0x100U,
                          semantic::FillStyleValue{semantic::PatternFillV1{
                              1, {1, 0, 0, 1}, semantic::PatternKind::kCrosshatch, -45, 8, 1}}},
                         {0x101U, semantic::StrokeStyleValue{stroke}},
                         {0x102U,
                          semantic::SketchStyleV1{1, 9223372036854775809ULL, rough, true}}}};
                    const auto paths = scene::sketchGeometry(shape, props);
                    if (!paths)
                        return 1;
                    for (const auto *path : {&paths->outline, &paths->hatches})
                        for (const auto &c : path->commands) {
                            if (!first)
                                std::cout << ',';
                            first = false;
                            std::cout << '[' << c.index();
                            std::visit(
                                [](const auto &c) {
                                    using T = std::decay_t<decltype(c)>;
                                    if constexpr (std::is_same_v<T, semantic::MoveTo>)
                                        std::cout << ',' << c.point.x << ',' << c.point.y;
                                    else if constexpr (!std::is_same_v<T, semantic::ClosePath>) {
                                        if constexpr (std::is_same_v<T, semantic::CubicTo>)
                                            std::cout << ',' << c.control1.x << ',' << c.control1.y
                                                      << ',' << c.control2.x << ',' << c.control2.y;
                                        std::cout << ',' << c.end.x << ',' << c.end.y;
                                    }
                                },
                                c);
                            std::cout << ']';
                        }
                }
    std::cout << "]\n";
}
