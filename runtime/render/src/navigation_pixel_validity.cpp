#include "navigation_pixel_validity.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <tuple>

namespace canvas::render::internal {
namespace {
using Rect=foundation::WorldRect;
bool overlap(Rect a,Rect b){return a.left<b.right&&a.right>b.left&&a.top<b.bottom&&a.bottom>b.top;}
float down(double x){return std::nextafter(static_cast<float>(x),-std::numeric_limits<float>::infinity());}
float up(double x){return std::nextafter(static_cast<float>(x),std::numeric_limits<float>::infinity());}
bool whole(const ReferenceTraversalEntry& e){
    if(const auto* c=std::get_if<CompactStrokeReferenceCommand>(&e.command))return c->content.get().stroke.config.algorithmVersion==4;
    if(const auto* c=std::get_if<BrushStrokeReferenceCommand>(&e.command))return c->content.get().stroke.snapshot.material_mode==1;
    return false;
}
bool bodyEqual(const ReferenceTraversalEntry& a,const ReferenceTraversalEntry& b){
    const auto& x=a.record;const auto& y=b.record;
    if(std::tie(x.objectId,x.kind,x.kindVersion,x.placement,x.transform,x.properties,x.content,x.derivedGeometry,
        x.geometryBounds,x.visualBounds,x.worldBounds,x.referenceGeometryDigest,x.directDependencies,x.geometryChunks,x.textLayout,x.hierarchyOrder)!=
       std::tie(y.objectId,y.kind,y.kindVersion,y.placement,y.transform,y.properties,y.content,y.derivedGeometry,
        y.geometryBounds,y.visualBounds,y.worldBounds,y.referenceGeometryDigest,y.directDependencies,y.geometryChunks,y.textLayout,y.hierarchyOrder)||
       a.contributesPixels!=b.contributesPixels||a.command!=b.command||
       a.presentationOpacity!=b.presentationOpacity||a.presentationEraseActive||b.presentationEraseActive||
       !a.presentationEraseMasks.empty()||!b.presentationEraseMasks.empty())return false;
    // Only these consumers always draw the complete immutable geometry. Other
    // chunk hints are part of their pixel identity and cannot be discarded.
    return (whole(a)&&whole(b))||std::tie(a.geometryChunkIndices,a.geometryChunked,a.navigationChunkSpan)==
        std::tie(b.geometryChunkIndices,b.geometryChunked,b.navigationChunkSpan);
}
struct Hull {
    double left=std::numeric_limits<double>::infinity(),top=left;
    double right=-left,bottom=-left;
    bool point(double x,double y){
        if(!std::isfinite(x)||!std::isfinite(y))return false;
        left=std::min(left,x);right=std::max(right,x);top=std::min(top,y);bottom=std::max(bottom,y);return true;
    }
};
bool addBounds(std::optional<Rect>& result,const semantic::EraseMaskRecord& mask,const semantic::Transform2D& t){
    Hull local;
    const bool valid=std::visit([&](const auto& g){
        if constexpr(requires{g.segments;}){
            if(g.segments.empty())return false;
            for(const auto& s:g.segments){
                if(!std::isfinite(s.p0.radius)||!std::isfinite(s.p1.radius)||s.p0.radius<=0||s.p1.radius<=0)return false;
                const double r=std::max(s.p0.radius,s.p1.radius);
                if(!std::isfinite(static_cast<float>(2*r)))return false;
                for(const auto p:{s.p0.position,s.p1.position,s.control1,s.control2}){
                    if(!std::isfinite(static_cast<float>(p.x))||!std::isfinite(static_cast<float>(p.y))||
                       !local.point(p.x-r,p.y-r)||!local.point(p.x+r,p.y+r))return false;
                }
            }
        }else{
            if(g.path.commands.empty()||(g.path.fill_rule!=semantic::FillRule::kNonZero&&g.path.fill_rule!=semantic::FillRule::kEvenOdd))return false;
            for(const auto& c:g.path.commands){bool ok=true;std::visit([&](const auto& v){
                if constexpr(requires{v.point;})ok=local.point(v.point.x,v.point.y);
                if constexpr(requires{v.end;})ok=ok&&local.point(v.end.x,v.end.y);
                if constexpr(requires{v.control;})ok=ok&&local.point(v.control.x,v.control.y);
                if constexpr(requires{v.control1;})ok=ok&&local.point(v.control1.x,v.control1.y)&&local.point(v.control2.x,v.control2.y);
            },c);if(!ok)return false;}
        }
        return std::isfinite(local.left)&&std::isfinite(local.top)&&std::isfinite(local.right)&&std::isfinite(local.bottom);
    },mask.geometry);
    if(!valid)return false;
    Hull world;
    for(const auto x:{local.left,local.right})for(const auto y:{local.top,local.bottom})
        if(!world.point(t.a*x+t.c*y+t.tx,t.b*x+t.d*y+t.ty))return false;
    for(const auto v:{t.a,t.b,t.c,t.d,t.tx,t.ty})if(!std::isfinite(static_cast<float>(v)))return false;
    // Bound float narrowing and multiply/add cancellation before AA padding.
    // Large cancelling coordinates deliberately lose reuse rather than
    // certifying a tiny double hull for a distant float-rendered mask.
    const double localMagnitude=std::max({std::abs(local.left),std::abs(local.right),std::abs(local.top),std::abs(local.bottom)});
    const double error=64*std::numeric_limits<float>::epsilon()*
        ((std::abs(t.a)+std::abs(t.b)+std::abs(t.c)+std::abs(t.d))*localMagnitude+std::abs(t.tx)+std::abs(t.ty));
    Rect r{down(world.left-error),down(world.top-error),up(world.right+error),up(world.bottom+error)};
    if(!r.isFiniteAndOrdered())return false;
    if(result){r={std::min(r.left,result->left),std::min(r.top,result->top),std::max(r.right,result->right),std::max(r.bottom,result->bottom)};}
    result=r;return true;
}
std::size_t commands(const semantic::EraseMaskRecord& m){return std::visit([](const auto& g){if constexpr(requires{g.segments;})return g.segments.size();else return g.path.commands.size();},m.geometry);}
}
bool NavigationRegions::add(Rect r){
    for(std::size_t i=0;i<count;){const auto a=areas[i];
        if(a.left<=r.left&&a.top<=r.top&&a.right>=r.right&&a.bottom>=r.bottom)return true;
        const bool contains=r.left<=a.left&&r.top<=a.top&&r.right>=a.right&&r.bottom>=a.bottom;
        const bool horizontal=a.top==r.top&&a.bottom==r.bottom&&a.left<=r.right&&r.left<=a.right;
        const bool vertical=a.left==r.left&&a.right==r.right&&a.top<=r.bottom&&r.top<=a.bottom;
        if(contains||horizontal||vertical){r={std::min(a.left,r.left),std::min(a.top,r.top),std::max(a.right,r.right),std::max(a.bottom,r.bottom)};areas[i]=areas[--count];i=0;}else ++i;
    }
    if(count==areas.size())return false;areas[count++]=r;return true;
}
bool NavigationRegions::subtract(Rect damage){
    if(!damage.isFiniteAndOrdered()){count=0;return false;}
    NavigationRegions next;
    const auto add=[&](float l,float t,float r,float b){return r<=l||b<=t||next.add({l,t,r,b});};
    for(std::size_t i=0;i<count;++i){const auto a=areas[i];if(!overlap(a,damage)){if(!next.add(a)){count=0;return false;}continue;}
        const Rect x{std::max(a.left,damage.left),std::max(a.top,damage.top),std::min(a.right,damage.right),std::min(a.bottom,damage.bottom)};
        if(!add(a.left,a.top,x.left,a.bottom)||!add(x.right,a.top,a.right,a.bottom)||
           !add(x.left,a.top,x.right,x.top)||!add(x.left,x.bottom,x.right,a.bottom)){count=0;return false;}
    }
    *this=next;return true;
}
MaskPixelDifference maskPixelDifference(std::span<const ReferenceTraversalEntry> before,std::span<const ReferenceTraversalEntry> after) noexcept {
    if(before.size()!=after.size()||before.size()>512)return {};
    MaskPixelDifference result{true,{}};std::size_t work=0;
    for(std::size_t i=0;i<before.size();++i){const auto& a=before[i];const auto& b=after[i];
        if(!bodyEqual(a,b))return {};
        const auto& x=a.record.eraseMasks;const auto& y=b.record.eraseMasks;
        if(x.size()>64||y.size()>64)return {};
        for(const auto* masks:{&x,&y})for(std::size_t j=0;j<masks->size();++j){
            const auto& mask=(*masks)[j];if(mask.id.isZero())return {};
            for(std::size_t k=0;k<j;++k)if((*masks)[k].id==mask.id)return {};
            const auto n=commands(mask);if(n>16384-work)return {};work+=n;
        }
        if(x==y)continue;
        if(!whole(a))return {};
        std::size_t last=0;bool common=false;
        for(const auto& old:x){const auto found=std::find_if(y.begin(),y.end(),[&](const auto& m){return m.id==old.id;});
            if(found!=y.end()){
                const auto j=std::size_t(found-y.begin());if(common&&j<=last)return {};last=j;common=true;
                if(old==*found)continue;
                if(!addBounds(result.damage,*found,b.record.transform))return {};
            }
            if(!addBounds(result.damage,old,a.record.transform))return {};
        }
        for(const auto& now:y)if(std::none_of(x.begin(),x.end(),[&](const auto& m){return m.id==now.id;}))
            if(!addBounds(result.damage,now,b.record.transform))return {};
    }
    return result;
}
std::optional<Rect> maskSamplingDamage(Rect r,double density,Rect rasterWorld) noexcept {
    if(!r.isFiniteAndOrdered()||!std::isfinite(density)||density<=0)return {};
    // One source AA pixel plus the bilinear sampling footprint; outward-rounded
    // bounds also cover the original float Skia transform/coordinate narrowing.
    const double origin=std::max({std::abs(double(r.left)),std::abs(double(r.top)),std::abs(double(r.right)),std::abs(double(r.bottom)),
        std::abs(double(rasterWorld.left)),std::abs(double(rasterWorld.top)),std::abs(double(rasterWorld.right)),std::abs(double(rasterWorld.bottom))});
    const double pad=2/density+64*std::numeric_limits<float>::epsilon()*origin;
    Rect result{down(double(r.left)-pad),down(double(r.top)-pad),up(double(r.right)+pad),up(double(r.bottom)+pad)};
    return result.isFiniteAndOrdered()?std::optional{result}:std::nullopt;
}
}
