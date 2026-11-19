#pragma once
#include "skia_erase_mask.hpp"
#include "canvas/foundation/internal/frame_timing.hpp"
#include <cmath>
#include <span>

namespace canvas::render::internal {
// Work selection only. Draw surviving primitives with the original full-surface
// clip and matrix: narrowing Skia's stroke clip changes antialiasing at edges.
// The cubic lies inside its control hull. Inflate by the larger endpoint radius
// before mapping, then by the same conservative physical AA/rounding guard used
// for feedback damage. Unknown transforms/paths keep sequential full replay.
inline bool feedbackMaskIntersects(const semantic::EraseCubicSegment& s,
    const SkMatrix& matrix,const SkIRect& damage) {
    if(!matrix.isFinite()||matrix.hasPerspective())return true;
    const double radius=std::max(s.p0.radius,s.p1.radius);
    auto bounds=SkRect::MakeLTRB(
        float(std::min({s.p0.position.x,s.p1.position.x,s.control1.x,s.control2.x})-radius),
        float(std::min({s.p0.position.y,s.p1.position.y,s.control1.y,s.control2.y})-radius),
        float(std::max({s.p0.position.x,s.p1.position.x,s.control1.x,s.control2.x})+radius),
        float(std::max({s.p0.position.y,s.p1.position.y,s.control1.y,s.control2.y})+radius));
    if(!bounds.isFinite()||!std::isfinite(radius)||radius<0)return true;
    bounds=matrix.mapRect(bounds);bounds.outset(3,3);
    return !bounds.isFinite()||SkRect::Intersects(bounds,SkRect::Make(damage));
}
inline void drawFeedbackDamageMasks(SkCanvas& canvas,
    std::span<const semantic::EraseMaskRecord> masks,const SkIRect& damage) {
    const auto matrix=canvas.getTotalMatrix();
    for(const auto& mask:masks){
        if(const auto* sweep=std::get_if<semantic::SweptCircleMask>(&mask.geometry)){
            for(const auto& segment:sweep->segments)
                if(feedbackMaskIntersects(segment,matrix,damage))drawErasePrimitive(canvas,segment);
        }else if(const auto* filled=std::get_if<semantic::FilledPathMask>(&mask.geometry))
            drawErasePrimitive(canvas,*filled);
    }
}
}
