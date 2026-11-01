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
    if(config.algorithmVersion<3&&config.family==foundation::InkFamily::kRound&&config.roundMode==foundation::InkRoundMode::kRegular)return config.size*.5;
    if(config.algorithmVersion>=3){
        auto t=.5+config.thinning*(double(pressure)/65535.-.5);
        switch(config.easing){
            case foundation::InkEasing::kEaseIn:t*=t;break;
            case foundation::InkEasing::kEaseOut:t=t*(2-t);break;
            case foundation::InkEasing::kSmoothstep:t=t*t*(3-2*t);break;
            default:break;
        }
        return config.size*std::max(.0001,t);
    }
    return config.size*(.25+.5*(double(pressure)/65535.0));
}
inline double compactLength(std::span<const foundation::InkKnot> knots) noexcept {
    double length=0;for(std::size_t i=1;i<knots.size();++i)
        length+=std::hypot(knots[i].x-knots[i-1].x,knots[i].y-knots[i-1].y);
    return length;
}
inline std::vector<CoverageKnot> compactCoverage(const foundation::InkBrushConfig& config,
    std::span<const foundation::InkKnot> knots,double startDistance=0,double totalDistance=-1) {
    std::vector<CoverageKnot> result;if(knots.empty())return result;
    if(totalDistance<0)totalDistance=startDistance+compactLength(knots);
    const bool taper=config.algorithmVersion==3&&totalDistance>1e-6;
    const auto envelope=[&](double at){
        const auto ease=[](double x){x=std::clamp(x,0.,1.);return x*x*(3-2*x);};
        return taper?std::min(config.startTaper>0?ease(at/config.startTaper):1.,
            config.endTaper>0?ease((totalDistance-at)/config.endTaper):1.):1.;
    };
    double distance=startDistance;
    result.push_back({knots.front().x,knots.front().y,compactRadius(config,knots.front().pressure)*envelope(distance)});
    for(std::size_t i=1;i<knots.size();++i){
        const auto& a=knots[i-1];const auto& b=knots[i];const auto length=std::hypot(b.x-a.x,b.y-a.y);
        // Taper envelopes are nonlinear. Subdivide just their transition region;
        // body geometry keeps the existing compact segment count.
        const bool transition=taper&&((config.startTaper>0&&distance<config.startTaper)||
            (config.endTaper>0&&distance+length>totalDistance-config.endTaper));
        std::vector<double> cuts{0,1};
        if(transition&&length>0){
            const auto add=[&](double from,double to){
                from=std::max(distance,from);to=std::min(distance+length,to);if(to<=from)return;
                const auto count=std::max(1U,static_cast<unsigned>(std::ceil((to-from)/std::max(.5,config.size/16))));
                for(unsigned j=0;j<=count;++j)cuts.push_back(std::clamp((from+(to-from)*double(j)/count-distance)/length,0.,1.));
            };
            if(config.startTaper>0)add(0,config.startTaper);
            if(config.endTaper>0)add(totalDistance-config.endTaper,totalDistance);
            std::sort(cuts.begin(),cuts.end());cuts.erase(std::unique(cuts.begin(),cuts.end()),cuts.end());
        }
        for(std::size_t j=1;j<cuts.size();++j){const auto t=cuts[j];
            const auto pressure=static_cast<std::uint16_t>(std::lround(a.pressure+(double(b.pressure)-a.pressure)*t));
            result.push_back({a.x+(b.x-a.x)*t,a.y+(b.y-a.y)*t,compactRadius(config,pressure)*envelope(distance+length*t)});
        }
        distance+=length;
    }
    return result;
}
}
