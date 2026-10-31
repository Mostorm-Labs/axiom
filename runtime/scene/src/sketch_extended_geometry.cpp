#include "canvas/semantic/table_content.hpp"
#include "sketch_geometry_internal.hpp"
#include <algorithm>
#include <bit>
#include <cmath>
#include <deque>
#include <limits>
#include <string>
namespace canvas::scene::detail {
namespace {
using namespace semantic;
constexpr std::uint64_t outlineDomain = 0x706174682d763031ULL, fillDomain = 0x68617463682d7631ULL;
std::uint64_t mixed(std::uint64_t n) {
    n = (n ^ (n >> 30)) * 0xbf58476d1ce4e5b9ULL;
    n = (n ^ (n >> 27)) * 0x94d049bb133111ebULL;
    return n ^ (n >> 31);
}
struct Style {
    std::uint64_t seed = 0;
    double roughness = 0;
    bool present = false, dashed = false, valid = true;
    std::optional<PatternFillV1> fill;
};
Style style(const PropertyBag &bag) {
    Style s;
    for (const auto &e : bag.entries) {
        if (const auto *v = std::get_if<SketchStyleV1>(&e.value)) {
            s.present = true;
            s.valid = s.valid && v->version == 1 && v->seed != 0 && std::isfinite(v->roughness) &&
                      v->roughness >= 0 && v->roughness <= 3;
            s.seed = v->seed;
            s.roughness = v->enabled ? v->roughness : 0;
        }
        if (const auto *f = std::get_if<FillStyleValue>(&e.value))
            if (const auto *p = std::get_if<PatternFillV1>(f)) {
                s.fill = *p;
                s.valid =
                    s.valid && p->version == 1 &&
                    (p->kind == PatternKind::kDiagonal || p->kind == PatternKind::kCrosshatch) &&
                    std::isfinite(p->angle) && p->angle >= -180 && p->angle <= 180 &&
                    std::isfinite(p->gap) && p->gap >= 2 && p->gap <= 1024 &&
                    std::isfinite(p->width) && p->width >= .1 && p->width <= 64;
            }
        if (const auto *v = std::get_if<StrokeStyleValue>(&e.value))
            if (const auto *p = std::get_if<SolidStroke>(v))
                s.dashed = std::holds_alternative<DashPattern>(p->dash);
    }
    return s;
}
void number(std::string &key, std::uint64_t value) {
    for (int i = 0; i < 8; ++i)
        key.push_back(static_cast<char>(value >> (i * 8)));
}
void number(std::string &key, double value) {
    number(key, std::bit_cast<std::uint64_t>(value == 0 ? 0. : value));
}
void point(std::string &key, Vec2 p) {
    number(key, p.x);
    number(key, p.y);
}
std::string key(const ObjectContent &content, const Style &s) {
    std::string out("extended-sketch-v1");
    number(out, std::uint64_t(content.index()));
    number(out, s.seed);
    number(out, s.roughness);
    out += s.dashed;
    out += s.fill.has_value();
    if (s.fill) {
        number(out, std::uint64_t(s.fill->kind));
        number(out, s.fill->angle);
        number(out, s.fill->gap);
        number(out, s.fill->width);
    }
    if (const auto *path = std::get_if<VectorPathContent>(&content)) {
        number(out, std::uint64_t(path->geometry.fill_rule));
        for (const auto &command : path->geometry.commands) {
            out += static_cast<char>(command.index());
            std::visit(
                [&](const auto &c) {
                    using T = std::decay_t<decltype(c)>;
                    if constexpr (std::is_same_v<T, MoveTo>)
                        point(out, c.point);
                    else if constexpr (!std::is_same_v<T, ClosePath>) {
                        if constexpr (std::is_same_v<T, QuadTo>)
                            point(out, c.control);
                        else if constexpr (std::is_same_v<T, CubicTo>) {
                            point(out, c.control1);
                            point(out, c.control2);
                        }
                        point(out, c.end);
                    }
                },
                command);
        }
    } else if (const auto *table = std::get_if<TableContent>(&content)) {
        for (const auto *axes : {&table->rows, &table->columns}) {
            number(out, std::uint64_t(axes->size()));
            for (const auto &a : *axes) {
                out.append(reinterpret_cast<const char *>(a.id.bytes.data()), a.id.bytes.size());
                number(out, a.size);
            }
        }
    } else if (const auto *section = std::get_if<SectionContent>(&content)) {
        const auto &b = section->boundary;
        number(out, b.x);
        number(out, b.y);
        number(out, b.width);
        number(out, b.height);
    }
    return out;
}
void include(foundation::WorldRect &b, Vec2 p, bool &have) {
    const auto x = static_cast<float>(p.x), y = static_cast<float>(p.y);
    if (!have) {
        b = {x, y, x, y};
        have = true;
    } else {
        b.left = std::min(b.left, x);
        b.top = std::min(b.top, y);
        b.right = std::max(b.right, x);
        b.bottom = std::max(b.bottom, y);
    }
}
foundation::WorldRect pathBounds(const VectorPathGeometry &path) {
    foundation::WorldRect bounds{};
    bool have = false;
    for (const auto &command : path.commands)
        std::visit(
            [&](const auto &c) {
                using T = std::decay_t<decltype(c)>;
                if constexpr (std::is_same_v<T, MoveTo>)
                    include(bounds, c.point, have);
                else if constexpr (!std::is_same_v<T, ClosePath>) {
                    include(bounds, c.end, have);
                    if constexpr (std::is_same_v<T, QuadTo>)
                        include(bounds, c.control, have);
                    else if constexpr (std::is_same_v<T, CubicTo>) {
                        include(bounds, c.control1, have);
                        include(bounds, c.control2, have);
                    }
                }
            },
            command);
    return bounds;
}
void rectangle(VectorPathGeometry &body, double x, double y, double w, double h) {
    body.commands = {MoveTo{{x, y}}, LineTo{{x + w, y}}, LineTo{{x + w, y + h}}, LineTo{{x, y + h}},
                     ClosePath{}};
}
void outline(SketchGeometry &g, const Style &s) {
    if (s.roughness == 0) {
        g.outline = g.body;
        return;
    }
    const int passes = s.dashed ? 1 : 2;
    g.outlineWidthScale = passes == 2 ? 1 - .25 * std::min(s.roughness, 1.) : 1;
    for (int pass = 0; pass < passes; ++pass) {
        Vec2 current{}, start{};
        std::uint64_t state = 0;
        std::uint64_t subpath = 0;
        for (const auto &command : g.body.commands) {
            if (const auto *move = std::get_if<MoveTo>(&command)) {
                current = start = move->point;
                state = mixed(s.seed ^ outlineDomain ^ mixed(++subpath) ^
                              mixed(std::uint64_t(pass + 1)));
                g.outline.commands.push_back(*move);
            } else if (const auto *line = std::get_if<LineTo>(&command)) {
                g.outline.commands.push_back(sketchSegment(current, line->end, s.roughness, state));
                current = line->end;
            } else if (std::holds_alternative<ClosePath>(command)) {
                if (current != start)
                    g.outline.commands.push_back(sketchSegment(current, start, s.roughness, state));
                g.outline.commands.push_back(ClosePath{});
                current = start;
            } else {
                CubicTo cubic;
                if (const auto *q = std::get_if<QuadTo>(&command)) {
                    cubic = {{current.x + (q->control.x - current.x) * 2 / 3,
                              current.y + (q->control.y - current.y) * 2 / 3},
                             {q->end.x + (q->control.x - q->end.x) * 2 / 3,
                              q->end.y + (q->control.y - q->end.y) * 2 / 3},
                             q->end};
                } else
                    cubic = std::get<CubicTo>(command);
                const auto length =
                    std::hypot(current.x - cubic.control1.x, current.y - cubic.control1.y) +
                    std::hypot(cubic.control1.x - cubic.control2.x,
                               cubic.control1.y - cubic.control2.y) +
                    std::hypot(cubic.control2.x - cubic.end.x, cubic.control2.y - cubic.end.y);
                const auto noise = std::min(2., length / 10) * s.roughness;
                cubic.control1.x += sketchRandom(state) * noise;
                cubic.control1.y += sketchRandom(state) * noise;
                cubic.control2.x += sketchRandom(state) * noise;
                cubic.control2.y += sketchRandom(state) * noise;
                g.outline.commands.push_back(cubic);
                current = cubic.end;
            }
        }
    }
}
// These paths are a pattern source, used together with the exact canonical body
// clip. This retains Bézier holes and winding rules without polygon flattening.
void hatch(SketchGeometry &g, const Style &s) {
    if (!s.fill)
        return;
    const auto b = pathBounds(g.body);
    const auto &f = *s.fill;
    const int families = f.kind == PatternKind::kCrosshatch ? 2 : 1;
    for (int family = 0; family < families; ++family) {
        const auto angle = (f.angle + family * 90) * 3.14159265358979323846 / 180;
        const auto ux = std::round(std::cos(angle) * 1e12) / 1e12,
                   uy = std::round(std::sin(angle) * 1e12) / 1e12, nx = -uy, ny = ux;
        double lo = std::numeric_limits<double>::infinity(), hi = -lo, lowT = lo, highT = -lo;
        for (double x : {double(b.left), double(b.right)})
            for (double y : {double(b.top), double(b.bottom)}) {
                lo = std::min(lo, x * nx + y * ny);
                hi = std::max(hi, x * nx + y * ny);
                lowT = std::min(lowT, x * ux + y * uy);
                highT = std::max(highT, x * ux + y * uy);
            }
        const auto stride =
            std::max(1., std::ceil((std::ceil((hi - lo) / f.gap) + 1) * families / 8192));
        const auto gap = f.gap * stride, first = std::ceil(lo / gap) * gap;
        auto random = mixed(s.seed ^ fillDomain ^ mixed(std::uint64_t(family + 1)));
        for (int i = 0; i < 8192 / families; ++i) {
            const auto d = first + i * gap;
            if (d > hi)
                break;
            Vec2 a{nx * d + ux * lowT, ny * d + uy * lowT},
                z{nx * d + ux * highT, ny * d + uy * highT};
            g.hatches.commands.push_back(MoveTo{a});
            if (s.roughness == 0)
                g.hatches.commands.push_back(LineTo{z});
            else
                g.hatches.commands.push_back(sketchSegment(a, z, s.roughness * .3, random));
        }
    }
}
std::uint64_t lineSeed(std::uint64_t seed, bool horizontal, const ObjectId &preceding) {
    auto n = seed ^ (horizontal ? 0x7461626c652d726fULL : 0x7461626c652d636fULL);
    for (auto b : preceding.bytes)
        n = mixed(n ^ b);
    return n;
}
void grid(SketchGeometry &g, const TableContent &table, const Style &s) {
    const auto width = tableWidth(table), height = tableHeight(table);
    rectangle(g.body, 0, 0, width, height);
    const int passes = s.roughness > 0 ? 2 : 1;
    g.outlineWidthScale = passes == 2 ? 1 - .25 * std::min(s.roughness, 1.) : 1;
    for (bool horizontal : {true, false}) {
        const auto &axes = horizontal ? table.rows : table.columns;
        double position = 0;
        for (std::size_t i = 0; i <= axes.size(); ++i) {
            const auto id = i ? axes[i - 1].id : ObjectId{};
            const auto a = horizontal ? Vec2{0, position} : Vec2{position, 0};
            const auto b = horizontal ? Vec2{width, position} : Vec2{position, height};
            auto random = lineSeed(s.seed, horizontal, id);
            for (int pass = 0; pass < passes; ++pass) {
                g.outline.commands.push_back(MoveTo{a});
                if (s.roughness == 0)
                    g.outline.commands.push_back(LineTo{b});
                else
                    g.outline.commands.push_back(sketchSegment(a, b, s.roughness, random));
            }
            if (i < axes.size())
                position += axes[i].size;
        }
    }
}
std::shared_ptr<const SketchGeometry> generate(const ObjectContent &content, const Style &s) {
    auto g = std::make_shared<SketchGeometry>();
    if (const auto *path = std::get_if<VectorPathContent>(&content)) {
        g->body = path->geometry;
        outline(*g, s);
        hatch(*g, s);
    } else if (const auto *table = std::get_if<TableContent>(&content))
        grid(*g, *table, s);
    else if (const auto *section = std::get_if<SectionContent>(&content)) {
        const auto &b = section->boundary;
        rectangle(g->body, b.x, b.y, b.width, b.height);
        outline(*g, s);
        hatch(*g, s);
    } else
        return {};
    g->bounds = foundation::unionRects(pathBounds(g->body), pathBounds(g->outline));
    return g;
}
std::size_t commandCount(const SketchGeometry &g) {
    return g.body.commands.size() + g.outline.commands.size() + g.hatches.commands.size() +
           g.arrowFill.commands.size();
}
} // namespace
std::shared_ptr<const SketchGeometry> extendedSketchGeometry(const ObjectContent &content,
                                                             const PropertyBag &properties,std::size_t maximumCommands) {
    const auto s = style(properties);
    if (!s.valid || (!s.present && !s.fill))
        return {};
    if (const auto *path = std::get_if<VectorPathContent>(&content);
        path && path->geometry.commands.size() > maximumCommands)
        return {};
    const auto identity = key(content, s);
    struct Entry {
        std::string key;
        std::shared_ptr<const SketchGeometry> value;
    };
    thread_local std::deque<Entry> cache;
    thread_local std::size_t commands = 0;
    for (const auto &e : cache)
        if (e.key == identity)
            return e.value;
    auto value = generate(content, s);
    if (!value)
        return {};
    const auto count = commandCount(*value);
    if (count > 32768)
        return {};
    while (!cache.empty() && (cache.size() >= 64 || commands + count > 131072)) {
        commands -= commandCount(*cache.front().value);
        cache.pop_front();
    }
    cache.push_back({identity, value});
    commands += count;
    return value;
}
} // namespace canvas::scene::detail
