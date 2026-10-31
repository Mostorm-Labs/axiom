#pragma once
#include "canvas/runtime/ink_brush.hpp"
#include <span>
#include <vector>
#include <algorithm>

namespace canvas::ink {
struct CoverageKnot final {double x=0,y=0,radius=0;};
inline foundation::InkKnot inkMidpoint(const foundation::InkKnot& a,const foundation::InkKnot& b) noexcept {
    return {a.x+(b.x-a.x)*.5,a.y+(b.y-a.y)*.5,
            static_cast<std::uint16_t>((unsigned(a.pressure)+b.pressure+1)/2)};
}
// Midpoint quadratic fitting stays inside the control polygon. Flatten only
// the bounded newly completed segment; persist this fitted centreline once.
inline void appendInkQuadratic(std::vector<foundation::InkKnot>& out,
    const foundation::InkKnot& a,const foundation::InkKnot& control,
    const foundation::InkKnot& b,unsigned depth=0){
    const auto mid=inkMidpoint(a,b);
    if(depth>=8 || (std::hypot(control.x-mid.x,control.y-mid.y)<=.3 &&
        std::abs(double(control.pressure)-mid.pressure)<=655.35)){
        if(out.empty()||out.back()!=b)out.push_back(b);
        return;
    }
    const auto left=inkMidpoint(a,control),right=inkMidpoint(control,b),split=inkMidpoint(left,right);
    appendInkQuadratic(out,a,left,split,depth+1);
    appendInkQuadratic(out,split,right,b,depth+1);
}
// RDP with an independent pressure interpolation constraint. Coordinates stay
// binary64 and pressure is quantized before both preview and persistence.
inline std::vector<foundation::InkKnot> simplifyInkKnots(std::span<const foundation::InkKnot> input,double epsilon=.25,std::uint64_t* visits=nullptr) {
    if(input.size()<3)return {input.begin(),input.end()};
    std::vector<bool> keep(input.size());keep.front()=keep.back()=true;
    std::vector<std::pair<std::size_t,std::size_t>> work{{0,input.size()-1}};
    while(!work.empty()){
        const auto [first,last]=work.back();work.pop_back();
        const auto& a=input[first];const auto& b=input[last];const auto dx=b.x-a.x,dy=b.y-a.y,den=dx*dx+dy*dy;
        double worst=1;std::size_t selected=first;
        for(std::size_t i=first+1;i<last;++i){if(visits)++*visits;const auto& k=input[i];const auto t=den>1e-20?std::clamp(((k.x-a.x)*dx+(k.y-a.y)*dy)/den,0.,1.):double(i-first)/double(last-first);
            const auto distance=std::hypot(k.x-a.x-t*dx,k.y-a.y-t*dy)/epsilon;
            const auto pressure=std::abs(double(k.pressure)-(a.pressure+(double(b.pressure)-a.pressure)*t))/(.019*65535);
            const auto error=std::max(distance,pressure);if(error>worst){worst=error;selected=i;}
        }
        if(selected!=first){keep[selected]=true;work.emplace_back(first,selected);work.emplace_back(selected,last);}
    }
    std::vector<foundation::InkKnot> result;
    for(std::size_t i=0;i<input.size();++i)if(keep[i])result.push_back(input[i]);
    return result;
}
inline double compactRadius(const foundation::InkBrushConfig& config,std::uint16_t pressure) noexcept {
    if(config.family==foundation::InkFamily::kRound&&config.roundMode==foundation::InkRoundMode::kRegular)return config.size*.5;
    return config.size*(.25+.5*(double(pressure)/65535.0));
}
inline std::vector<CoverageKnot> compactCoverage(const foundation::InkBrushConfig& config,std::span<const foundation::InkKnot> knots) {
    std::vector<CoverageKnot> result;result.reserve(knots.size());
    for(const auto& k:knots)result.push_back({k.x,k.y,compactRadius(config,k.pressure)});
    return result;
}
}
