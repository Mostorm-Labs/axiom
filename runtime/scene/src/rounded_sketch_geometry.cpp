#include "canvas/scene/corner_geometry.hpp"
#include "sketch_geometry_internal.hpp"
#include <algorithm>
#include <bit>
#include <cmath>
#include <deque>
#include <string>
namespace canvas::scene::detail {
namespace {
using namespace semantic;
void number(std::string &s, std::uint64_t n) {
    for (unsigned i = 0; i < 8; ++i)
        s.push_back(static_cast<char>(n >> (i * 8)));
}
void number(std::string &s, double n) { number(s, std::bit_cast<std::uint64_t>(n == 0 ? 0. : n)); }
bool intersects(Vec2 a, const CubicTo &c, std::span<const foundation::WorldRect> obstacles) {
    const double l = std::min({a.x, c.control1.x, c.control2.x, c.end.x}),
                 r = std::max({a.x, c.control1.x, c.control2.x, c.end.x}),
                 t = std::min({a.y, c.control1.y, c.control2.y, c.end.y}),
                 b = std::max({a.y, c.control1.y, c.control2.y, c.end.y});
    for (const auto &o : obstacles)
        if (l < o.right && r > o.left && t < o.bottom && b > o.top)
            return true;
    return false;
}
Vec2 blend(Vec2 a, Vec2 b, double factor) {
    return {a.x + (b.x - a.x) * factor, a.y + (b.y - a.y) * factor};
}
void terminal(Vec2 origin, Vec2 direction, Vec2 &control) {
    const auto length = std::hypot(direction.x, direction.y);
    if (length == 0)
        return;
    direction = {direction.x / length, direction.y / length};
    const auto d =
        std::max(0., (control.x - origin.x) * direction.x + (control.y - origin.y) * direction.y);
    control = {origin.x + d * direction.x, origin.y + d * direction.y};
}
} // namespace
std::shared_ptr<const SketchGeometry> roundedSketchGeometry(const ObjectContent &content,
                                                            const PropertyBag &properties,
                                                            const ConnectorGeometry *route,
                                                            double radius) {
    if (!std::isfinite(radius) || radius <= 0 || radius > 16384)
        return {};
    VectorPathGeometry path;
    std::string identity("rounded-corners-v1");
    number(identity, std::uint64_t(content.index()));
    number(identity, radius);
    bool connector = std::holds_alternative<ConnectorContent>(content);
    if (const auto *shape = std::get_if<ShapeContent>(&content)) {
        if (shape->shape_kind != 1 && shape->shape_kind != 3)
            return {};
        const auto w = shape->width, h = shape->height;
        const auto points = shape->shape_kind == 3
                                ? std::vector<Vec2>{{w / 2, 0}, {w, h}, {0, h}}
                                : std::vector<Vec2>{{0, 0}, {w, 0}, {w, h}, {0, h}};
        path = roundedPolyline(points, true, radius);
        number(identity, std::uint64_t(shape->shape_kind));
        number(identity, w);
        number(identity, h);
    } else if (connector && route) {
        const auto points = route->route.empty()
                                ? std::vector<foundation::WorldPoint>{route->start, route->end}
                                : route->route;
        std::vector<Vec2> p;
        for (auto v : points) {
            p.push_back({v.x, v.y});
            number(identity, double(v.x));
            number(identity, double(v.y));
        }
        path = roundedPolyline(p, false, radius, route->sketchObstacles);
        number(identity, std::uint64_t(4));
        for (const auto &o : route->sketchObstacles) {
            number(identity, double(o.left));
            number(identity, double(o.top));
            number(identity, double(o.right));
            number(identity, double(o.bottom));
        }
    } else
        return {};
    if (path.commands.empty())
        return {};
    PropertyBag style;
    bool haveSketch = false;
    for (const auto &e : properties.entries) {
        if (e.field_id != 0x103U)
            style.entries.push_back(e);
        if (const auto *s = std::get_if<SketchStyleV1>(&e.value)) {
            haveSketch = true;
            identity += 'S';
            number(identity, std::uint64_t(s->version));
            number(identity, s->seed);
            number(identity, s->roughness);
            identity += s->enabled;
        }
        if (const auto *f = std::get_if<FillStyleValue>(&e.value))
            if (const auto *p = std::get_if<PatternFillV1>(f)) {
                identity += 'P';
                number(identity, std::uint64_t(p->version));
                number(identity, std::uint64_t(p->kind));
                number(identity, p->angle);
                number(identity, p->gap);
                number(identity, p->width);
            }
        if (const auto *s = std::get_if<StrokeStyleValue>(&e.value))
            if (const auto *v = std::get_if<SolidStroke>(s)) {
                identity += 'D';
                identity += std::holds_alternative<DashPattern>(v->dash);
            }
        if (const auto *d = std::get_if<ConnectorDecorationValue>(&e.value)) {
            identity += 'A';
            number(identity, std::uint64_t(e.field_id));
            number(identity, std::uint64_t(*d));
        }
    }
    if (!haveSketch)
        style.entries.push_back({0x102U, SketchStyleV1{1, 1, 0, false}});
    struct Entry {
        std::string key;
        std::shared_ptr<const SketchGeometry> value;
        std::size_t count;
    };
    thread_local std::deque<Entry> cache;
    thread_local std::size_t commands = 0;
    for (const auto &e : cache)
        if (e.key == identity)
            return e.value;
    const auto base = extendedSketchGeometry(VectorPathContent{path}, style, 8192);
    if (!base)
        return {};
    auto g = std::make_shared<SketchGeometry>(*base);
    if (connector) {
        Vec2 current{};
        for (std::size_t i = 0; i < g->outline.commands.size(); ++i) {
            const auto index = i % path.commands.size();
            const auto &original = path.commands[index];
            auto &output = g->outline.commands[i];
            if (const auto *move = std::get_if<MoveTo>(&original)) {
                current = move->point;
                continue;
            }
            const auto *line = std::get_if<LineTo>(&original);
            const auto *cubic = std::get_if<CubicTo>(&original);
            const auto end = line ? line->end : cubic ? cubic->end : current;
            if (auto *curve = std::get_if<CubicTo>(&output)) {
                const auto exact =
                    cubic ? *cubic
                          : CubicTo{blend(current, end, 1. / 3), blend(current, end, 2. / 3), end};
                if (index == 1)
                    terminal(current, {end.x - current.x, end.y - current.y}, curve->control1);
                if (index + 1 == path.commands.size())
                    terminal(end, {current.x - end.x, current.y - end.y}, curve->control2);
                const auto perturbed = *curve;
                bool safe = false;
                double factor = 1;
                for (unsigned attempt = 0; attempt < 9; ++attempt) {
                    curve->control1 = blend(exact.control1, perturbed.control1, factor);
                    curve->control2 = blend(exact.control2, perturbed.control2, factor);
                    if (!intersects(current, *curve, route->sketchObstacles)) {
                        safe = true;
                        break;
                    }
                    factor *= .5;
                }
                if (!safe)
                    *curve = exact;
            }
            current = end;
        }
        // Reuse v1 arrow geometry/random sequence; only its route outline is replaced.
        auto arrowStyle = style;
        bool enabled = false;
        for (auto &e : arrowStyle.entries)
            if (auto *s = std::get_if<SketchStyleV1>(&e.value)) {
                enabled = s->enabled && s->roughness > 0;
                if (!s->enabled) {
                    s->enabled = true;
                    s->roughness = 0;
                }
            }
        const auto arrows = sketchGeometry(content, arrowStyle, route);
        if (arrows) {
            g->arrowFill = arrows->arrowFill;
            bool dashed = false;
            for (const auto &e : style.entries)
                if (const auto *s = std::get_if<StrokeStyleValue>(&e.value))
                    if (const auto *v = std::get_if<SolidStroke>(s))
                        dashed = std::holds_alternative<DashPattern>(v->dash);
            const auto n = route->route.empty() ? 2 : route->route.size();
            const auto count = (1 + 2 * (n - 1)) * (enabled && !dashed ? 2 : 1);
            if (count < arrows->outline.commands.size())
                g->outline.commands.insert(g->outline.commands.end(),
                                           arrows->outline.commands.begin() +
                                               static_cast<std::ptrdiff_t>(count),
                                           arrows->outline.commands.end());
            g->bounds = foundation::unionRects(g->bounds, arrows->bounds);
        }
    }
    const auto count = g->body.commands.size() + g->outline.commands.size() +
                       g->hatches.commands.size() + g->arrowFill.commands.size();
    if (count > 32768)
        return {};
    while (!cache.empty() && (cache.size() >= 64 || commands + count > 131072)) {
        commands -= cache.front().count;
        cache.pop_front();
    }
    cache.push_back({std::move(identity), g, count});
    commands += count;
    return g;
}
} // namespace canvas::scene::detail
