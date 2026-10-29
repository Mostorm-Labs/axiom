#pragma once
#include "canvas/semantic/object_content.hpp"
#include "include/core/SkPathBuilder.h"
#include "include/core/SkPaint.h"
#include "include/effects/SkDashPathEffect.h"
#include <type_traits>
namespace canvas::render::internal {
inline SkPath semanticPath(const semantic::VectorPathGeometry& geometry) {
    SkPathBuilder path(geometry.fill_rule==semantic::FillRule::kEvenOdd?SkPathFillType::kEvenOdd:SkPathFillType::kWinding);
    const auto f=[](double v){return static_cast<float>(v);};
    for(const auto& command:geometry.commands)std::visit([&](const auto& v) {
        using T=std::decay_t<decltype(v)>;
        if constexpr(std::is_same_v<T,semantic::MoveTo>)path.moveTo(f(v.point.x),f(v.point.y));
        else if constexpr(std::is_same_v<T,semantic::LineTo>)path.lineTo(f(v.end.x),f(v.end.y));
        else if constexpr(std::is_same_v<T,semantic::QuadTo>)path.quadTo(f(v.control.x),f(v.control.y),f(v.end.x),f(v.end.y));
        else if constexpr(std::is_same_v<T,semantic::CubicTo>)path.cubicTo(f(v.control1.x),f(v.control1.y),f(v.control2.x),f(v.control2.y),f(v.end.x),f(v.end.y));
        else if constexpr(std::is_same_v<T,semantic::ClosePath>)path.close();
    },command);
    return path.detach();
}
inline SkPaint geometryStrokePaint(const semantic::SolidStroke& stroke,bool antiAlias=false) {
    SkPaint p;p.setAntiAlias(antiAlias);p.setColor4f({stroke.color.r,stroke.color.g,stroke.color.b,stroke.color.a});
    p.setStyle(SkPaint::kStroke_Style);p.setStrokeWidth(static_cast<float>(stroke.width));
    switch(stroke.cap) {
    case semantic::StrokeCap::kButt:p.setStrokeCap(SkPaint::kButt_Cap);break;
    case semantic::StrokeCap::kRound:p.setStrokeCap(SkPaint::kRound_Cap);break;
    case semantic::StrokeCap::kSquare:p.setStrokeCap(SkPaint::kSquare_Cap);break;
    }
    std::visit([&](const auto& j){using J=std::decay_t<decltype(j)>;
        if constexpr(std::is_same_v<J,semantic::RoundJoin>)p.setStrokeJoin(SkPaint::kRound_Join);
        else if constexpr(std::is_same_v<J,semantic::BevelJoin>)p.setStrokeJoin(SkPaint::kBevel_Join);
        else {p.setStrokeJoin(SkPaint::kMiter_Join);p.setStrokeMiter(static_cast<float>(j.limit));}
    },stroke.join);
    if(const auto* d=std::get_if<semantic::DashPattern>(&stroke.dash)) {
        std::vector<SkScalar> intervals;for(auto v:d->segments)intervals.push_back(static_cast<float>(v));
        p.setPathEffect(SkDashPathEffect::Make(SkSpan<const SkScalar>(intervals.data(),intervals.size()),static_cast<float>(d->offset)));
    }
    return p;
}
}
