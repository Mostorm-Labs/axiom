#include "canvas/scene/corner_geometry.hpp"
#include "canvas/semantic/object_record.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <numbers>
namespace canvas::scene {
namespace {
using semantic::Vec2;
Vec2 add(Vec2 a, Vec2 b) { return {a.x + b.x, a.y + b.y}; }
Vec2 sub(Vec2 a, Vec2 b) { return {a.x - b.x, a.y - b.y}; }
Vec2 mul(Vec2 a, double t) { return {a.x * t, a.y * t}; }
double cross(Vec2 a, Vec2 b) { return a.x * b.y - a.y * b.x; }
double dot(Vec2 a, Vec2 b) { return a.x * b.x + a.y * b.y; }
double length(Vec2 a) { return std::hypot(a.x, a.y); }
struct Corner {
    Vec2 entry{}, exit{}, c1{}, c2{};
    bool curve = false;
};
bool blocked(const Corner &c, std::span<const foundation::WorldRect> obstacles) {
    const double l = std::min({c.entry.x, c.exit.x, c.c1.x, c.c2.x}),
                 r = std::max({c.entry.x, c.exit.x, c.c1.x, c.c2.x});
    const double t = std::min({c.entry.y, c.exit.y, c.c1.y, c.c2.y}),
                 b = std::max({c.entry.y, c.exit.y, c.c1.y, c.c2.y});
    for (const auto &o : obstacles)
        if (l < o.right && r > o.left && t < o.bottom && b > o.top)
            return true;
    return false;
}
Vec2 cubic(Vec2 p, Vec2 a, Vec2 b, Vec2 z, double t) {
    const auto s = 1 - t;
    return add(add(mul(p, s * s * s), mul(a, 3 * s * s * t)),
               add(mul(b, 3 * s * t * t), mul(z, t * t * t)));
}
std::array<Vec2,4> polygonVertices(const semantic::ShapeContent& shape) {
    const auto w=shape.width, h=shape.height;
    if(shape.shape_kind==3)return {{{w/2,0},{w,h},{0,h},{w/2,0}}};
    return {{{0,0},{w,0},{w,h},{0,h}}};
}
Vec2 perimeterOrigin(const semantic::ShapeContent& shape,double radius) {
    const auto w=shape.width, h=shape.height;
    if(shape.shape_kind==3 && radius>0)return {w/2,h-w*h/(w+2*std::hypot(w/2,h))};
    return {w/2,h/2};
}
} // namespace
semantic::VectorPathGeometry roundedPolyline(std::span<const Vec2> input, bool closed,
                                             double radius,
                                             std::span<const foundation::WorldRect> obstacles,
                                             std::optional<std::array<Vec2,2>> shaftEnds) {
    semantic::VectorPathGeometry out;
    if (input.empty() || input.size() > 4097 || !std::isfinite(radius) || radius < 0)
        return out;
    std::vector<Vec2> points;
    points.reserve(input.size());
    for (auto p : input) {
        if (!std::isfinite(p.x) || !std::isfinite(p.y))
            return {};
        if (points.empty() || points.back() != p)
            points.push_back(p);
    }
    if (closed && points.size() > 1 && points.back() == points.front())
        points.pop_back();
    if (points.empty())
        return out;
    const auto n = points.size();
    std::vector<double> factor(n, 0), leg(n, 0);
    for (std::size_t i = 0; i < n; ++i) {
        if (!closed && (i == 0 || i + 1 == n))
            continue;
        const auto a = sub(points[(i + n - 1) % n], points[i]),
                   b = sub(points[(i + 1) % n], points[i]);
        const auto la = length(a), lb = length(b);
        if (la == 0 || lb == 0)
            continue;
        const auto angle = std::acos(std::clamp(dot(a, b) / (la * lb), -1., 1.));
        if (angle < 1e-8 || std::abs(std::numbers::pi - angle) < 1e-8)
            continue;
        factor[i] = 1 / std::tan(angle / 2);
        leg[i] = std::min(la, lb) / 2;
        if (!closed && shaftEnds) {
            if (i==1) leg[i]=std::min(leg[i],length(sub((*shaftEnds)[0],points[i])));
            if (i+2==n) leg[i]=std::min(leg[i],length(sub((*shaftEnds)[1],points[i])));
        }
    }
    double effective = radius;
    if (closed)
        for (std::size_t i = 0; i < n; ++i) {
            const auto j = (i + 1) % n; const auto s = factor[i] + factor[j];
            if (s > 0)
                effective = std::min(effective, length(sub(points[j], points[i])) / s);
        }
    std::vector<Corner> corners(n);
    for (std::size_t i = 0; i < n; ++i) {
        auto &c = corners[i];
        c.entry = c.exit = points[i];
        if (factor[i] == 0 || effective == 0)
            continue;
        auto trim = effective * factor[i];
        if (!closed)
            trim = std::min(trim, leg[i]);
        const auto a = sub(points[(i + n - 1) % n], points[i]),
                   b = sub(points[(i + 1) % n], points[i]);
        const auto u = mul(a, 1 / length(a)), v = mul(b, 1 / length(b));
        const auto angle = std::acos(std::clamp(dot(u, v), -1., 1.));
        for (unsigned attempt = 0; attempt < 9; ++attempt) {
            const auto actualRadius = trim / factor[i],
                       k = 4. / 3 * std::tan((std::numbers::pi - angle) / 4) * actualRadius;
            c.entry = add(points[i], mul(u, trim));
            c.exit = add(points[i], mul(v, trim));
            c.c1 = sub(c.entry, mul(u, k));
            c.c2 = sub(c.exit, mul(v, k));
            c.curve = trim > 1e-9;
            if (!blocked(c, obstacles))
                break;
            c.curve = false;
            c.entry = c.exit = points[i];
            trim *= .5;
        }
    }
    if (!closed && shaftEnds && n>=2) {
        corners.front().entry=corners.front().exit=(*shaftEnds)[0];
        corners.back().entry=corners.back().exit=(*shaftEnds)[1];
    }
    out.commands.push_back(semantic::MoveTo{corners[0].entry});
    for (std::size_t i = 0; i < n; ++i) {
        const auto &c = corners[i];
        if (i)
            out.commands.push_back(semantic::LineTo{c.entry});
        if (c.curve)
            out.commands.push_back(semantic::CubicTo{c.c1, c.c2, c.exit});
    }
    if (closed) {
        out.commands.push_back(semantic::LineTo{corners[0].entry});
        out.commands.push_back(semantic::ClosePath{});
    }
    // Preserve the exact legacy command topology at zero radius.
    if (radius == 0 && closed && out.commands.size() > 1)
        out.commands.erase(out.commands.end() - 2);
    return out;
}
std::optional<Vec2> nearestShapePerimeterHint(const semantic::ShapeContent& shape,double radius,
                                               const semantic::Transform2D& world,foundation::WorldPoint pointer) {
    const auto w=shape.width, h=shape.height;
    if(!(w>0 && h>0) || shape.shape_kind<1 || shape.shape_kind>3)return {};
    const auto delta=[&](Vec2 p)->Vec2 {
        return {world.a*p.x+world.c*p.y+(world.tx-pointer.x),
                world.b*p.x+world.d*p.y+(world.ty-pointer.y)};
    };
    const auto distance=[&](Vec2 p) { const auto d=delta(p);return dot(d,d); };
    double best=std::numeric_limits<double>::infinity();
    Vec2 closest{};
    const auto consider=[&](Vec2 p) {
        const auto d=distance(p);
        if(d<best){best=d;closest=p;}
        return d;
    };
    const auto segment=[&](Vec2 a,Vec2 b) {
        const auto from=delta(a), v=sub(delta(b),from);
        const double length2=dot(v,v);
        if(length2>0)consider(add(a,mul(sub(b,a),std::clamp(-dot(from,v)/length2,0.,1.))));
    };
    // Each curved piece is one ellipse quadrant or one cubic fillet. Bracket
    // its distance minima in world space, then refine directly on the curve.
    const auto arc = [&](const auto& at) {
        constexpr int steps = 8;
        constexpr double goldenSection = .6180339887498948482;
        std::array<double, steps+1> samples;
        for (int i=0; i<=steps; ++i) samples[i] = consider(at(double(i)/steps));
        for (int i=0; i<=steps; ++i) {
            if ((i>0 && samples[i]>samples[i-1]) || (i<steps && samples[i]>samples[i+1])) continue;
            double lo = double(std::max(0,i-1))/steps;
            double hi = double(std::min(steps,i+1))/steps;
            double left = hi-goldenSection*(hi-lo), right = lo+goldenSection*(hi-lo);
            double leftCost = distance(at(left)), rightCost = distance(at(right));
            for (int n=0; n<34; ++n) {
                if (leftCost <= rightCost) {
                    hi = right;
                    right = left;
                    rightCost = leftCost;
                    left = hi-goldenSection*(hi-lo);
                    leftCost = distance(at(left));
                } else {
                    lo = left;
                    left = right;
                    leftCost = rightCost;
                    right = lo+goldenSection*(hi-lo);
                    rightCost = distance(at(right));
                }
            }
            consider(at((lo+hi)*.5));
        }
    };
    if(shape.shape_kind==2) {
        for(int i=0;i<4;++i)arc([&](double t)->Vec2 {
            const double theta=(i+t)*(std::numbers::pi/2);
            return {w*.5*(1+std::cos(theta)),h*.5*(1+std::sin(theta))};
        });
    } else if(radius<=0) {
        const auto vertices=polygonVertices(shape);
        for(std::size_t i=0;i<vertices.size();++i)segment(vertices[i],vertices[(i+1)%vertices.size()]);
    } else {
        const auto path=roundedPolyline(polygonVertices(shape),true,radius);
        Vec2 previous{},start{};
        for(const auto& command:path.commands) {
            if(const auto* m=std::get_if<semantic::MoveTo>(&command))previous=start=m->point;
            else if(const auto* c=std::get_if<semantic::CubicTo>(&command)) {
                arc([&](double t){return cubic(previous,c->control1,c->control2,c->end,t);});
                previous=c->end;
            } else {
                const auto to=std::get_if<semantic::LineTo>(&command);
                const auto end=to ? to->end : start;
                segment(previous,end);previous=end;
            }
        }
    }
    if(!std::isfinite(best))return {};
    const auto direction=sub(closest,perimeterOrigin(shape,radius));
    const double nx=direction.x/w,ny=direction.y/h,extent=std::max(std::abs(nx),std::abs(ny));
    if(!(extent>0))return {};
    return Vec2{.5+.5*(nx/extent),.5+.5*(ny/extent)};
}
bool roundedShapePerimeter(const semantic::ShapeContent &shape, double radius, Vec2 direction,
                           Vec2 &position, Vec2 &normal) {
    if (radius <= 0 || (shape.shape_kind != 1 && shape.shape_kind != 3) || shape.width <= 0 ||
        shape.height <= 0 || length(direction) == 0)
        return false;
    const auto path = roundedPolyline(polygonVertices(shape),true,radius);
    const auto origin = perimeterOrigin(shape,radius);
    direction = mul(direction, 1 / length(direction));
    double best = std::numeric_limits<double>::infinity();
    Vec2 previous{}, start{};
    const auto accept = [&](Vec2 p, Vec2 tangent) {
        const auto distance = dot(sub(p, origin), direction);
        if (distance >= 0 && distance < best) {
            best = distance;
            position = p;
            normal = {tangent.y, -tangent.x};
        }
    };
    for (const auto &cmd : path.commands) {
        if (const auto *m = std::get_if<semantic::MoveTo>(&cmd)) {
            previous = start = m->point;
            continue;
        }
        if (const auto *c = std::get_if<semantic::CubicTo>(&cmd)) {
            const auto value = [&](double t) {
                return cross(sub(cubic(previous, c->control1, c->control2, c->end, t), origin),
                             direction);
            };
            double lo = 0, a = value(0);
            for (unsigned step = 1; step <= 32; ++step) {
                double hi = double(step) / 32, b = value(hi);
                if (a == 0 || b == 0 || std::signbit(a) != std::signbit(b)) {
                    double left = lo, right = hi;
                    for (unsigned k = 0; k < 40; ++k) {
                        const auto mid = (left + right) / 2, v = value(mid);
                        if ((a < 0) == (v < 0))
                            left = mid;
                        else
                            right = mid;
                    }
                    const auto t = a == 0 ? lo : b == 0 ? hi : (left + right) / 2, s = 1 - t;
                    const auto derivative = add(add(mul(sub(c->control1, previous), 3 * s * s),
                                                    mul(sub(c->control2, c->control1), 6 * s * t)),
                                                mul(sub(c->end, c->control2), 3 * t * t));
                    accept(cubic(previous, c->control1, c->control2, c->end, t), derivative);
                }
                lo = hi;
                a = b;
            }
            previous = c->end;
        } else {
            const auto end = std::holds_alternative<semantic::ClosePath>(cmd)
                                 ? start
                                 : std::get<semantic::LineTo>(cmd).end;
            const auto tangent = sub(end, previous); const auto denominator = cross(tangent, direction);
            if (std::abs(denominator) > 1e-12) {
                const auto t = cross(sub(origin, previous), direction) / denominator;
                if (t >= 0 && t <= 1)
                    accept(add(previous, mul(tangent, t)), tangent);
            }
            previous = end;
        }
    }
    return std::isfinite(best) && length(normal) > 0;
}
} // namespace canvas::scene
