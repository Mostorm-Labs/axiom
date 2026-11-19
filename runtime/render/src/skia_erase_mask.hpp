#pragma once
#include "skia_geometry_internal.hpp"
#include "canvas/semantic/erase_mask.hpp"
#include "include/core/SkCanvas.h"

namespace canvas::render::internal {
#if defined(AXIOM_TRANSIENT_DISPLAY_TESTS)
struct EraseFeedbackDrawWork {std::uint64_t sweptDraws=0,filledDraws=0;};
inline thread_local EraseFeedbackDrawWork eraseFeedbackDrawWork;
#endif
inline void drawErasePrimitive(SkCanvas& canvas,const semantic::EraseCubicSegment& s){
#if defined(AXIOM_TRANSIENT_DISPLAY_TESTS)
    ++eraseFeedbackDrawWork.sweptDraws;
#endif
    SkPaint paint;paint.setAntiAlias(true);paint.setBlendMode(SkBlendMode::kClear);
    paint.setStyle(SkPaint::kStroke_Style);paint.setStrokeCap(SkPaint::kRound_Cap);paint.setStrokeJoin(SkPaint::kRound_Join);
    paint.setStrokeWidth(float(2*std::max(s.p0.radius,s.p1.radius)));
    SkPathBuilder b;b.moveTo(float(s.p0.position.x),float(s.p0.position.y));
    b.cubicTo(float(s.control1.x),float(s.control1.y),float(s.control2.x),float(s.control2.y),float(s.p1.position.x),float(s.p1.position.y));
    canvas.drawPath(b.detach(),paint);
}
inline void drawErasePrimitive(SkCanvas& canvas,const semantic::FilledPathMask& mask){
#if defined(AXIOM_TRANSIENT_DISPLAY_TESTS)
    ++eraseFeedbackDrawWork.filledDraws;
#endif
    SkPaint paint;paint.setAntiAlias(false);paint.setBlendMode(SkBlendMode::kClear);canvas.drawPath(semanticPath(mask.path),paint);
}
}
