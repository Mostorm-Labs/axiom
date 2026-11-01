#include "canvas/scene/sketch_geometry.hpp"
#include "sketch_geometry_internal.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <deque>
#include <limits>
#include <string>
namespace canvas::scene {
namespace {
using namespace semantic;
constexpr double pi = 3.14159265358979323846;
struct Random {
    std::uint64_t state;
    double next() {
        auto z = (state += 0x9e3779b97f4a7c15ULL);
        z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
        z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
        z ^= z >> 31;
        return static_cast<double>(z >> 11) * 0x1p-53 * 2 - 1;
    }
};
struct Key {
    int kind = 0;
    double w = 0, h = 0, rough = 0;
    std::uint64_t seed = 0;
    bool dashed = false, arrowStart = false, arrowEnd = false;
    std::optional<PatternFillV1> fill;
    std::vector<foundation::WorldPoint> route;
    std::vector<foundation::WorldRect> obstacles;
};
void number(std::string &key, std::uint64_t n) {
    for (int i = 0; i < 8; ++i)
        key.push_back(static_cast<char>(n >> (i * 8)));
}
void number(std::string &key, double n) {
    number(key, std::bit_cast<std::uint64_t>(n == 0 ? 0. : n));
}
std::string cacheKey(const Key &k) {
    std::string s;
    number(s, static_cast<std::uint64_t>(k.kind));
    number(s, k.w);
    number(s, k.h);
    number(s, k.rough);
    number(s, k.seed);
    s += k.dashed;
    s += k.arrowStart;
    s += k.arrowEnd;
    s += k.fill.has_value();
    if (k.fill) {
        number(s, static_cast<std::uint64_t>(k.fill->kind));
        number(s, k.fill->angle);
        number(s, k.fill->gap);
        number(s, k.fill->width);
    }
    number(s, static_cast<std::uint64_t>(k.route.size()));
    for (auto p : k.route) {
        number(s, static_cast<double>(p.x));
        number(s, static_cast<double>(p.y));
    }
    for (auto r : k.obstacles) {
        number(s, static_cast<double>(r.left));
        number(s, static_cast<double>(r.top));
        number(s, static_cast<double>(r.right));
        number(s, static_cast<double>(r.bottom));
    }
    return s;
}
bool intrudes(Vec2 a, Vec2 b, Vec2 c, Vec2 d, const std::vector<foundation::WorldRect> &obstacles) {
    const auto l = std::min({a.x, b.x, c.x, d.x}), r = std::max({a.x, b.x, c.x, d.x});
    const auto t = std::min({a.y, b.y, c.y, d.y}), v = std::max({a.y, b.y, c.y, d.y});
    for (const auto &o : obstacles)
        if (l < o.right && r > o.left && t < o.bottom && v > o.top)
            return true;
    return false;
}
CubicTo segment(Vec2 a, Vec2 b, double rough, Random &random, bool tangentStart, bool tangentEnd,
                const std::vector<foundation::WorldRect> &obstacles) {
    const auto dx = b.x - a.x, dy = b.y - a.y, length = std::hypot(dx, dy),
               noise = std::min(2., length / 10) * rough;
    // Draw all coefficients even at zero roughness: continuous parameter mapping.
    const auto bow = random.next() * noise, rx = random.next() * noise, ry = random.next() * noise;
    const auto sx = random.next() * noise, sy = random.next() * noise;
    const Vec2 base1{a.x + dx / 3, a.y + dy / 3}, base2{a.x + dx * 2 / 3, a.y + dy * 2 / 3};
    Vec2 delta1{rx - (length ? dy / length * bow : 0), ry + (length ? dx / length * bow : 0)};
    Vec2 delta2{sx - (length ? dy / length * bow : 0), sy + (length ? dx / length * bow : 0)};
    const auto project = [&](Vec2 v) {
        const double t = length ? (v.x * dx + v.y * dy) / (length * length) : 0;
        return Vec2{dx * t, dy * t};
    };
    if (tangentStart)
        delta1 = project(delta1);
    if (tangentEnd)
        delta2 = project(delta2);
    for (int attempt = 0; attempt < 5; ++attempt) {
        const double scale = attempt == 4 ? 0 : std::ldexp(1., -attempt);
        Vec2 c1{base1.x + delta1.x * scale, base1.y + delta1.y * scale},
            c2{base2.x + delta2.x * scale, base2.y + delta2.y * scale};
        if (scale == 0 || !intrudes(a, c1, c2, b, obstacles))
            return {c1, c2, b};
    }
    return {base1, base2, b};
}
void addPolygon(VectorPathGeometry &p, const std::vector<Vec2> &points) {
    if (points.empty())
        return;
    p.commands.push_back(MoveTo{points[0]});
    for (std::size_t i = 1; i < points.size(); ++i)
        p.commands.push_back(LineTo{points[i]});
    p.commands.push_back(ClosePath{});
}
VectorPathGeometry body(const Key &k) {
    VectorPathGeometry p;
    if (k.kind == 2) {
        constexpr double q = .5522847498307936;
        const double rx = k.w / 2, ry = k.h / 2;
        p.commands = {MoveTo{{k.w, ry}},
                      CubicTo{{k.w, ry + q * ry}, {rx + q * rx, k.h}, {rx, k.h}},
                      CubicTo{{rx - q * rx, k.h}, {0, ry + q * ry}, {0, ry}},
                      CubicTo{{0, ry - q * ry}, {rx - q * rx, 0}, {rx, 0}},
                      CubicTo{{rx + q * rx, 0}, {k.w, ry - q * ry}, {k.w, ry}},
                      ClosePath{}};
    } else if (k.kind == 3)
        addPolygon(p, {{k.w / 2, 0}, {k.w, k.h}, {0, k.h}});
    else if (k.kind == 1)
        addPolygon(p, {{0, 0}, {k.w, 0}, {k.w, k.h}, {0, k.h}});
    return p;
}
void hatch(SketchGeometry &g, const Key &k) {
    if (!k.fill)
        return;
    Random fillRandom{k.seed ^ 0x66696c6c2d763031ULL};
    const auto &f = *k.fill;
    const int families = f.kind == PatternKind::kCrosshatch ? 2 : 1;
    for (int family = 0; family < families; ++family) {
        double angle = (f.angle + family * 90) * pi / 180;
        double ux = std::round(std::cos(angle) * 1e12) / 1e12,
               uy = std::round(std::sin(angle) * 1e12) / 1e12, nx = -uy, ny = ux;
        std::vector<Vec2> vertices =
            k.kind == 3 ? std::vector<Vec2>{{k.w / 2, 0}, {k.w, k.h}, {0, k.h}}
                        : std::vector<Vec2>{{0, 0}, {k.w, 0}, {k.w, k.h}, {0, k.h}};
        double lo = std::numeric_limits<double>::infinity(), hi = -lo;
        for (auto p : vertices) {
            double d = p.x * nx + p.y * ny;
            lo = std::min(lo, d);
            hi = std::max(hi, d);
        }
        const double count = std::ceil((hi - lo) / f.gap) + 1;
        const double stride = std::max(1., std::ceil(count * families / 8192));
        const double gap = f.gap * stride;
        const double first = std::ceil(lo / gap) * gap;
        for (int n = 0; n < 8192 / families; ++n) {
            double d = first + n * gap;
            if (d > hi)
                break;
            double t0 = 0, t1 = 0;
            bool valid = false;
            if (k.kind == 2) {
                const double rx = k.w / 2, ry = k.h / 2, cx = rx, cy = ry, px = nx * d - cx,
                             py = ny * d - cy;
                const double A = ux * ux / (rx * rx) + uy * uy / (ry * ry),
                             B = 2 * (px * ux / (rx * rx) + py * uy / (ry * ry)),
                             C = px * px / (rx * rx) + py * py / (ry * ry) - 1,
                             disc = B * B - 4 * A * C;
                if (disc > 0) {
                    t0 = (-B - std::sqrt(disc)) / (2 * A);
                    t1 = (-B + std::sqrt(disc)) / (2 * A);
                    valid = true;
                }
            } else {
                double low = std::numeric_limits<double>::infinity(), high = -low;
                int hits = 0;
                for (std::size_t i = 0; i < vertices.size(); ++i) {
                    auto a = vertices[i], b = vertices[(i + 1) % vertices.size()];
                    double da = a.x * nx + a.y * ny, db = b.x * nx + b.y * ny;
                    if (std::abs(db - da) < 1e-12)
                        continue;
                    double v = (d - da) / (db - da);
                    if (v < 0 || v > 1)
                        continue;
                    double t = (a.x + (b.x - a.x) * v) * ux + (a.y + (b.y - a.y) * v) * uy;
                    low = std::min(low, t);
                    high = std::max(high, t);
                    ++hits;
                }
                if (hits >= 2 && high > low) {
                    t0 = low;
                    t1 = high;
                    valid = true;
                }
            }
            if (valid) {
                const Vec2 start{nx * d + ux * t0, ny * d + uy * t0},
                    end{nx * d + ux * t1, ny * d + uy * t1};
                g.hatches.commands.push_back(MoveTo{start});
                if (k.rough == 0)
                    g.hatches.commands.push_back(LineTo{end});
                else
                    g.hatches.commands.push_back(
                        segment(start, end, k.rough * .3, fillRandom, false, false, {}));
            }
        }
    }
}
void bounds(SketchGeometry &g) {
    bool have = false;
    auto add = [&](Vec2 p) {
        const float x = static_cast<float>(p.x), y = static_cast<float>(p.y);
        if (!have) {
            g.bounds = {x, y, x, y};
            have = true;
        } else {
            g.bounds.left = std::min(g.bounds.left, x);
            g.bounds.top = std::min(g.bounds.top, y);
            g.bounds.right = std::max(g.bounds.right, x);
            g.bounds.bottom = std::max(g.bounds.bottom, y);
        }
    };
    for (const auto *path : {&g.body, &g.outline, &g.arrowFill})
        for (const auto &cmd : path->commands)
            std::visit(
                [&](const auto &c) {
                    using T = std::decay_t<decltype(c)>;
                    if constexpr (std::is_same_v<T, MoveTo>)
                        add(c.point);
                    else if constexpr (!std::is_same_v<T, ClosePath>) {
                        add(c.end);
                        if constexpr (std::is_same_v<T, CubicTo>) {
                            add(c.control1);
                            add(c.control2);
                        }
                    }
                },
                cmd);
}
std::shared_ptr<const SketchGeometry> generate(const Key &k) {
    auto g = std::make_shared<SketchGeometry>();
    g->body = body(k);
    Random rng{k.seed ^ 0x6f75746c696e6531ULL};
    const int passes = k.rough > 0 && !k.dashed ? 2 : 1;
    g->outlineWidthScale = passes == 2 ? 1 - .25 * std::min(k.rough, 1.) : 1;
    if (k.kind == 4) {
        if (k.route.size() > 4097)
            return {};
        for (int pass = 0; pass < passes; ++pass) {
            if (k.route.size() < 2)
                break;
            auto p = k.route[0];
            g->outline.commands.push_back(MoveTo{{p.x, p.y}});
            for (std::size_t i = 1; i < k.route.size(); ++i) {
                auto a = k.route[i - 1], b = k.route[i];
                const Vec2 mid{(double(a.x) + b.x) / 2, (double(a.y) + b.y) / 2};
                g->outline.commands.push_back(
                    segment({a.x, a.y}, mid, k.rough, rng, i == 1, false, k.obstacles));
                g->outline.commands.push_back(segment(mid, {b.x, b.y}, k.rough, rng, false,
                                                      i + 1 == k.route.size(), k.obstacles));
            }
        }
        const auto arrow = [&](std::size_t tipIndex, std::size_t prevIndex) {
            auto tip = k.route[tipIndex], prev = k.route[prevIndex];
            double dx = tip.x - prev.x, dy = tip.y - prev.y, len = std::hypot(dx, dy);
            if (len == 0)
                return;
            double ux = dx / len, uy = dy / len;
            std::vector<Vec2> v{{tip.x, tip.y},
                                {tip.x - ux * 12 - uy * 6, tip.y - uy * 12 + ux * 6},
                                {tip.x - ux * 12 + uy * 6, tip.y - uy * 12 - ux * 6}};
            addPolygon(g->arrowFill, v);
            for (std::size_t i = 0; i < 3; ++i) {
                g->outline.commands.push_back(MoveTo{v[i]});
                g->outline.commands.push_back(
                    segment(v[i], v[(i + 1) % 3], k.rough, rng, false, false, {}));
            }
        };
        if (k.route.size() > 1) {
            if (k.arrowEnd)
                arrow(k.route.size() - 1, k.route.size() - 2);
            if (k.arrowStart)
                arrow(0, 1);
        }
    } else if (k.rough == 0) {
        g->outline = g->body;
    } else if (k.kind == 2) {
        for (int pass = 0; pass < passes; ++pass) {
            Vec2 start{};
            g->outline.commands.push_back(g->body.commands.front());
            start = std::get<MoveTo>(g->body.commands.front()).point;
            for (const auto &cmd : g->body.commands)
                if (const auto *c = std::get_if<CubicTo>(&cmd)) {
                    auto next = *c;
                    const auto noise = std::min({2., k.w / 10, k.h / 10}) * k.rough;
                    next.control1.x += rng.next() * noise;
                    next.control1.y += rng.next() * noise;
                    next.control2.x += rng.next() * noise;
                    next.control2.y += rng.next() * noise;
                    g->outline.commands.push_back(next);
                }
            g->outline.commands.push_back(ClosePath{});
            (void)start;
        }
    } else {
        std::vector<Vec2> v = k.kind == 3
                                  ? std::vector<Vec2>{{k.w / 2, 0}, {k.w, k.h}, {0, k.h}}
                                  : std::vector<Vec2>{{0, 0}, {k.w, 0}, {k.w, k.h}, {0, k.h}};
        for (int pass = 0; pass < passes; ++pass)
            for (std::size_t i = 0; i < v.size(); ++i) {
                g->outline.commands.push_back(MoveTo{v[i]});
                g->outline.commands.push_back(
                    segment(v[i], v[(i + 1) % v.size()], k.rough, rng, false, false, {}));
            }
    }
    hatch(*g, k);
    bounds(*g);
    return g;
}
 } // namespace
namespace detail {
double sketchRandom(std::uint64_t& state){Random random{state};const auto value=random.next();state=random.state;return value;}
semantic::CubicTo sketchSegment(semantic::Vec2 a,semantic::Vec2 b,double rough,std::uint64_t& state){Random random{state};const auto value=segment(a,b,rough,random,false,false,{});state=random.state;return value;}
}
std::shared_ptr<const SketchGeometry> sketchGeometry(const semantic::ObjectContent &content,
                                                     const semantic::PropertyBag &properties,
                                                     const ConnectorGeometry *route) {
    for(const auto& p:properties.entries)if(const auto* corner=std::get_if<semantic::CornerStyleV1>(&p.value);corner&&corner->radius>0){if(corner->version!=1)return {};return detail::roundedSketchGeometry(content,properties,route,corner->radius);}
    if(std::holds_alternative<semantic::VectorPathContent>(content)||std::holds_alternative<semantic::TableContent>(content)||std::holds_alternative<semantic::SectionContent>(content))return detail::extendedSketchGeometry(content,properties);
    Key k;
    const SketchStyleV1 *sketch = nullptr;
    for (const auto &e : properties.entries) {
        if (const auto *s = std::get_if<SketchStyleV1>(&e.value))
            sketch = s;
        if (const auto *f = std::get_if<FillStyleValue>(&e.value))
            if (const auto *p = std::get_if<PatternFillV1>(f)) {
                k.fill = *p;
                k.fill->color = {};
            }
        if (const auto *s = std::get_if<StrokeStyleValue>(&e.value))
            if (const auto *p = std::get_if<SolidStroke>(s))
                k.dashed = std::holds_alternative<DashPattern>(p->dash);
        if (const auto *d = std::get_if<ConnectorDecorationValue>(&e.value)) {
            if (e.field_id == 0x200)
                k.arrowStart = *d == ConnectorDecorationValue::kArrow;
            if (e.field_id == 0x201)
                k.arrowEnd = *d == ConnectorDecorationValue::kArrow;
        }
    }
    if ((!sketch || !sketch->enabled) && !k.fill)
        return {};
    if (sketch) {
        if (sketch->version != 1 || sketch->seed == 0)
            return {};
        k.seed = sketch->seed;
        k.rough = sketch->enabled ? sketch->roughness : 0;
    }
    if (const auto *s = std::get_if<ShapeContent>(&content)) {
        k.kind = static_cast<int>(s->shape_kind);
        k.w = s->width;
        k.h = s->height;
    } else if (const auto *s = std::get_if<StickyContent>(&content)) {
        k.kind = 1;
        k.w = s->width;
        k.h = s->height;
    } else if (std::holds_alternative<ConnectorContent>(content) && route) {
        k.kind = 4;
        k.route = route->route;
        k.obstacles = route->sketchObstacles;
        if (k.route.empty())
            k.route = {route->start, route->end};
    } else
        return {};
    if (k.kind != 4 && (!std::isfinite(k.w) || !std::isfinite(k.h) || k.w <= 0 || k.h <= 0))
        return {};
    if (k.fill && (k.fill->version != 1 || !std::isfinite(k.fill->gap) || k.fill->gap < 2))
        return {};
    const auto key = cacheKey(k);
    struct Entry {
        std::string key;
        std::shared_ptr<const SketchGeometry> value;
    };
    thread_local std::deque<Entry> cache;
    thread_local std::size_t commands = 0;
    for (const auto &e : cache)
        if (e.key == key)
            return e.value;
    auto value = generate(k);
    if (!value)
        return {};
    const auto n = value->outline.commands.size() + value->hatches.commands.size();
    while (!cache.empty() && (cache.size() >= 64 || commands + n > 131072)) {
        commands -= cache.front().value->outline.commands.size() +
                    cache.front().value->hatches.commands.size();
        cache.pop_front();
    }
    cache.push_back({key, value});
    commands += n;
    return value;
}
} // namespace canvas::scene
