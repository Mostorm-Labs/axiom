#include "canvas/interaction/region_selection_geometry.hpp"
#include "canvas/scene/derived_geometry.hpp"

#include <algorithm>
#include <array>
#include <cmath>

namespace canvas::interaction {
namespace {
constexpr double epsilon=1e-9;
double cross(RegionPoint a,RegionPoint b,RegionPoint c) noexcept {
    return (b.x-a.x)*(c.y-a.y)-(b.y-a.y)*(c.x-a.x);
}
bool onSegment(RegionPoint p,RegionPoint a,RegionPoint b) noexcept {
    return std::abs(cross(a,b,p))<=epsilon &&
        p.x>=std::min(a.x,b.x)-epsilon&&p.x<=std::max(a.x,b.x)+epsilon&&
        p.y>=std::min(a.y,b.y)-epsilon&&p.y<=std::max(a.y,b.y)+epsilon;
}
bool inside(RegionPoint p,std::span<const RegionPoint> polygon) noexcept {
    bool result=false;
    for (std::size_t i=0,j=polygon.size()-1;i<polygon.size();j=i++) {
        const auto a=polygon[j],b=polygon[i];
        if (onSegment(p,a,b)) return true;
        if ((a.y>p.y)!=(b.y>p.y) && p.x<(b.x-a.x)*(p.y-a.y)/(b.y-a.y)+a.x) result=!result;
    }
    return result;
}
bool segmentsIntersect(RegionPoint a,RegionPoint b,RegionPoint c,RegionPoint d) noexcept {
    const double ab1=cross(a,b,c),ab2=cross(a,b,d),cd1=cross(c,d,a),cd2=cross(c,d,b);
    if (((ab1>epsilon&&ab2<-epsilon)||(ab1<-epsilon&&ab2>epsilon))&&
        ((cd1>epsilon&&cd2<-epsilon)||(cd1<-epsilon&&cd2>epsilon))) return true;
    return onSegment(a,c,d)||onSegment(b,c,d)||onSegment(c,a,b)||onSegment(d,a,b);
}
bool polygonsIntersect(std::span<const RegionPoint> a,std::span<const RegionPoint> b) noexcept {
    for (auto p:a) if (inside(p,b)) return true;
    for (auto p:b) if (inside(p,a)) return true;
    for (std::size_t i=0;i<a.size();++i)
        for (std::size_t j=0;j<b.size();++j)
            if (segmentsIntersect(a[i],a[(i+1)%a.size()],b[j],b[(j+1)%b.size()])) return true;
    return false;
}
bool ellipseIntersects(std::span<const RegionPoint> points,double w,double h) noexcept {
    if (inside({w*.5,h*.5},points)) return true;
    const auto normalized=[&](RegionPoint p) {return RegionPoint{p.x/(w*.5)-1,p.y/(h*.5)-1};};
    for (std::size_t i=0;i<points.size();++i) {
        const auto a=normalized(points[i]),b=normalized(points[(i+1)%points.size()]);
        const double dx=b.x-a.x,dy=b.y-a.y,length=dx*dx+dy*dy;
        const double t=length>epsilon?std::clamp(-(a.x*dx+a.y*dy)/length,0.0,1.0):0;
        const double x=a.x+t*dx,y=a.y+t*dy;
        if (x*x+y*y<=1+epsilon) return true;
    }
    return false;
}
}
RegionIntersection intersectSelectionRegion(const semantic::ObjectRecord& object,
    std::span<const RegionPoint> region) noexcept {
    if (!validSelectionPolygon(region)) return RegionIntersection::kInvalid;
    if (!object.erase_masks.empty()) return RegionIntersection::kUnsupported;
    double w=0,h=0;unsigned kind=1;
    if (object.kind==semantic::ObjectKind::kShape) {
        const auto* content=std::get_if<semantic::ShapeContent>(&object.content);
        if (!content) return RegionIntersection::kInvalid;
        kind=content->shape_kind;w=content->width;h=content->height;
        if (kind<1||kind>3) return RegionIntersection::kUnsupported;
    } else if (object.kind==semantic::ObjectKind::kImage) {
        const auto* c=std::get_if<semantic::ImageContent>(&object.content);
        if (!c) return RegionIntersection::kInvalid;w=c->width;h=c->height;
    } else if (object.kind==semantic::ObjectKind::kSticky) {
        const auto* c=std::get_if<semantic::StickyContent>(&object.content);
        if (!c) return RegionIntersection::kInvalid;w=c->width;h=c->height;
    } else return RegionIntersection::kUnsupported;
    const auto& t=object.transform;const double det=t.a*t.d-t.b*t.c;
    if (!std::isfinite(w)||!std::isfinite(h)||w<=0||h<=0||!std::isfinite(det)||std::abs(det)<1e-12)
        return RegionIntersection::kInvalid;
    std::vector<RegionPoint> local;local.reserve(region.size());
    for (auto p:region) {
        const double x=p.x-t.tx,y=p.y-t.ty;
        const RegionPoint q{(t.d*x-t.c*y)/det,(-t.b*x+t.a*y)/det};
        if (!std::isfinite(q.x)||!std::isfinite(q.y)) return RegionIntersection::kInvalid;
        local.push_back(q);
    }
    bool hit=false;
    if (kind==2) hit=ellipseIntersects(local,w,h);
    else if (kind==3) {
        const auto triangle=scene::triangleGeometry(std::get<semantic::ShapeContent>(object.content));
        if (!triangle) return RegionIntersection::kInvalid;
        const std::array<RegionPoint,3> polygon{{{triangle->top.x,triangle->top.y},
            {triangle->right.x,triangle->right.y},{triangle->left.x,triangle->left.y}}};
        hit=polygonsIntersect(local,polygon);
    } else {
        const std::array<RegionPoint,4> polygon{{{0,0},{w,0},{w,h},{0,h}}};
        hit=polygonsIntersect(local,polygon);
    }
    return hit?RegionIntersection::kHit:RegionIntersection::kMiss;
}
} // namespace canvas::interaction
