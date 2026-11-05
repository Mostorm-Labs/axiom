#pragma once
#include "canvas/ink/compact_contour.hpp"
#include "include/core/SkPathBuilder.h"
#include "include/core/SkCanvas.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>

namespace canvas::render::internal {
// The same paired quadratic ribbon is used for coverage and acquisition.
inline SkPath compactContourRibbon(std::span<const ink::ContourPatch> patches) {
    SkPathBuilder path;
    if(patches.empty())return path.detach();
    const auto scalar=[](double v){return static_cast<SkScalar>(v);};
    const auto& start=patches.front();
    path.moveTo(scalar(start.leftStart.x),scalar(start.leftStart.y));
    for(const auto& p:patches)
        path.quadTo(scalar(p.leftControl.x),scalar(p.leftControl.y),scalar(p.leftEnd.x),scalar(p.leftEnd.y));
    const auto& last=patches.back();path.lineTo(scalar(last.rightEnd.x),scalar(last.rightEnd.y));
    for(auto i=patches.size();i-->0;){const auto& p=patches[i];
        path.quadTo(scalar(p.rightControl.x),scalar(p.rightControl.y),scalar(p.rightStart.x),scalar(p.rightStart.y));}
    path.close();return path.detach();
}
inline SkRect compactContourCapBounds(const ink::ContourCap& cap){
    const auto x=float(cap.x),y=float(cap.y),r=float(cap.radius);
    const auto inf=std::numeric_limits<float>::infinity();
    return SkRect::MakeLTRB(std::nextafter(x-r,-inf),std::nextafter(y-r,-inf),
                           std::nextafter(x+r,inf),std::nextafter(y+r,inf));
}
// Bounds of the exact paired paths/circles emitted by the pinned draw helper.
// Unknown/degenerate bounds must never authorize omission of a batch.
inline SkRect compactContourBounds(const ink::CompactContour& contour){
    auto bounds=SkRect::MakeEmpty();bool unknown=false;
    const auto join=[&](SkRect b){if(b.isEmpty()||!b.isFinite())unknown=true;else bounds.join(b);};
    for(std::size_t i=0;i+1<contour.patches.size();++i)
        join(compactContourRibbon(std::span(contour.patches).subspan(i,2)).getBounds());
    for(const auto& cap:contour.caps)join(compactContourCapBounds(cap));
    if(unknown){const auto max=std::numeric_limits<float>::max();return SkRect::MakeLTRB(-max,-max,max,max);}
    return bounds;
}
// Culling is a hint only. Preserve the actual raster clip/grid: narrowing it
// changes Skia analytic-AA even away from an edge. Both navigation and exact
// recorded coverage use this same conservative numeric-domain policy.
inline std::optional<SkRect> compactContourCullBounds(SkCanvas& canvas,const SkRect* deviceDemand=nullptr){
    auto clip=canvas.getLocalClipBounds();SkMatrix inverse;
    bool cull=!canvas.getTotalMatrix().hasPerspective()&&canvas.getTotalMatrix().invert(&inverse)&&clip.isFinite();
    if(cull&&deviceDemand){
        const auto demand=inverse.mapRect(*deviceDemand);
        if(!demand.isFinite())cull=false;
        else if(!clip.intersect(demand))clip=SkRect::MakeEmpty();
    }
    if(cull){
        const auto& matrix=canvas.getTotalMatrix();
        const auto scale=std::max(std::hypot(matrix.getScaleX(),matrix.getSkewY()),std::hypot(matrix.getSkewX(),matrix.getScaleY()));
        const auto origin=std::max({std::abs(clip.left()),std::abs(clip.top()),std::abs(clip.right()),std::abs(clip.bottom())});
        if(origin>65536||scale<1./64||scale>64||!std::isfinite(scale))cull=false;
        const auto margin=2*std::max(std::hypot(inverse.getScaleX(),inverse.getSkewY()),std::hypot(inverse.getSkewX(),inverse.getScaleY()));
        cull=cull&&std::isfinite(margin);if(cull)clip.outset(float(margin),float(margin));
    }
    return cull?std::optional<SkRect>(clip):std::nullopt;
}
}
