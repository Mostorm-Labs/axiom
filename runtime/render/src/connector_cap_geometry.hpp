#pragma once
#include "canvas/scene/derived_geometry.hpp"
#include "include/core/SkPaint.h"
#include "include/core/SkPathBuilder.h"
#include "include/effects/SkDashPathEffect.h"
#include <cmath>

namespace canvas::render::internal {
inline void applyConnectorWarningPaint(SkPaint& paint) {
    constexpr SkScalar intervals[]{6,4};
    paint.setColor4f({.9F,.35F,.12F,1},nullptr);
    paint.setPathEffect(SkDashPathEffect::Make(SkSpan<const SkScalar>(intervals,2),0));
}

// The normal stroke cap stays on free ends and dashes. When a decorated
// end is very thick, remove only the cap that would escape the triangle.
inline SkPath connectorCapOverhang(const scene::ConnectorGeometry& route, const SkPaint& paint,
                                   bool first, bool last) {
    if (paint.getStrokeCap()==SkPaint::kButt_Cap || route.status!=runtime::ConnectorRouteStatus::kReady ||
        route.route.size()<2 || !((first && route.arrow_start) || (last && route.arrow_end))) return {};
    SkPathBuilder cuts;
    for (bool start : {true,false}) {
        if (start ? !(first && route.arrow_start) : !(last && route.arrow_end)) continue;
        const std::size_t i=start ? 0 : route.route.size()-1;
        const auto tip=route.route[i];
        const auto base=scene::connectorShaftVertex(route.route,i,route.arrow_start,route.arrow_end);
        const float dx=tip.x-base.x, dy=tip.y-base.y, length=std::hypot(dx,dy);
        const float r=paint.getStrokeWidth()/2;
        // The head's half-width grows by one unit for every two units from its tip.
        if (length<=0 || 3*r<=length) continue;
        const float ux=dx/length, uy=dy/length, pad=r+1;
        const auto at=[&](float along,float across) -> SkPoint {
            return {base.x+ux*along-uy*across,base.y+uy*along+ux*across};
        };
        cuts.moveTo(at(0,-pad)); cuts.lineTo(at(length+pad,-pad));
        cuts.lineTo(at(length+pad,pad)); cuts.lineTo(at(0,pad)); cuts.close();
    }
    return cuts.detach();
}
} // namespace canvas::render::internal
