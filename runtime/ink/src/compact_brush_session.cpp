#include "canvas/ink/compact_brush_session.hpp"
#include "canvas/ink/compact_geometry.hpp"
#include <algorithm>
#include <cmath>

namespace canvas::ink {
CompactBrushSession::CompactBrushSession(CompactBrushConfig config,std::uint64_t seed) noexcept:config_(config),seed_(seed){}
bool CompactBrushSession::validSample(const TimedBrushSample& s,std::uint64_t sequence)const noexcept {
    return s.sequence>sequence&&std::isfinite(s.x)&&std::isfinite(s.y)&&
        (config_.pressureSource!=BrushPressureSource::kDevice||(s.pressureCapability&&s.pressurePresent&&std::isfinite(s.pressure)&&s.pressure>=0&&s.pressure<=1));
}
double CompactBrushSession::simulatedPressure(double distance,double dt,double pressure)const noexcept {
    if(dt<=0)return pressure;
    const auto t=std::clamp((distance/dt-config_.vLow)/(config_.vHigh-config_.vLow),0.,1.);
    const auto target=config_.pMax-(config_.pMax-config_.pMin)*t*t*(3-2*t);
    return std::clamp(pressure+(target-pressure)*(1-std::exp(-dt/config_.tauSeconds)),config_.pMin,config_.pMax);
}
bool CompactBrushSession::append(std::span<const TimedBrushSample> confirmed,std::span<const TimedBrushSample> predicted)noexcept {
    auto sequence=lastSequence_,time=lastTimestamp_;
    for(const auto& s:confirmed){if(!validSample(s,sequence)||(sequence&&s.timestampNs<time))return false;sequence=s.sequence;time=s.timestampNs;}
    for(const auto& s:predicted){if(!validSample(s,sequence)||(sequence&&s.timestampNs<time))return false;sequence=s.sequence;time=s.timestampNs;}
    if(confirmedCount_>262144||confirmed.size()>262144-confirmedCount_||predicted.size()>1024)return false;
    // All input admission precedes retained state changes. Prefix chunks are
    // immutable; only the bounded pending window is copied for publication.
    const auto retain=[&](const foundation::InkKnot& k){
        if(!pending_.empty())pendingLength_+=std::hypot(k.x-pending_.back().x,k.y-pending_.back().y);
        pending_.push_back(k);
        if(pending_.size()>=64||pendingLength_>=32){
            auto chunk=std::make_shared<BrushPreviewDelta::Chunk>();chunk->previous=chunks_;
            chunk->knots=simplifyInkKnots(pending_,config_.geometryEpsilon,&metrics_.simplificationVisits);
            metrics_.generatedKnots+=chunk->knots.size();chunks_=std::move(chunk);
            pending_={k};pendingLength_=0;
        }
    };
    for(const auto& s:confirmed){
        auto p=config_.pressureSource==BrushPressureSource::kDevice?s.pressure:previousPressure_;
        if(lastSequence_&&config_.pressureSource==BrushPressureSource::kSimulated)
            p=simulatedPressure(std::hypot(s.x-previousX_,s.y-previousY_),double(s.timestampNs-previousTime_)/1e9,previousPressure_);
        const foundation::InkKnot k{s.x==0?0:s.x,s.y==0?0:s.y,static_cast<std::uint16_t>(std::lround(p*65535))};
        if(config_.algorithmVersion==2){
            if(!confirmedCount_){retain(k);curveStart_=k;}
            else if(confirmedCount_>=2){
                if(confirmedCount_==2){curveStart_=inkMidpoint(previousControl_,lastKnot_);retain(curveStart_);}
                const auto end=inkMidpoint(lastKnot_,k);
                std::vector<foundation::InkKnot> segment;
                appendInkQuadratic(segment,curveStart_,lastKnot_,end);
                for(const auto& knot:segment)retain(knot);
                curveStart_=end;
            }
            previousControl_=lastKnot_;
        }else retain(k);
        lastKnot_=k;++confirmedCount_;++metrics_.samples;
        previousPressure_=p;
        if(!lastSequence_||s.timestampNs>previousTime_){previousX_=s.x;previousY_=s.y;previousTime_=s.timestampNs;}
        lastSequence_=s.sequence;lastTimestamp_=s.timestampNs;
    }
    auto tail=std::make_shared<BrushPreviewDelta::Chunk>();tail->previous=chunks_;
    auto pending=pending_;
    if(config_.algorithmVersion==2&&confirmedCount_&&pending.back()!=lastKnot_)pending.push_back(lastKnot_);
    tail->knots=simplifyInkKnots(pending,config_.geometryEpsilon,&metrics_.simplificationVisits);metrics_.copiedKnots+=pending.size();
    if(config_.algorithmVersion==2&&!predicted.empty()){
        auto speculative=*this;
        if(!speculative.append(predicted,{}))return false;
        preview_=speculative.preview_;return true;
    }
    auto pressure=previousPressure_;auto x=previousX_,y=previousY_;auto stamp=previousTime_;
    for(const auto& s:predicted){
        pressure=config_.pressureSource==BrushPressureSource::kDevice?s.pressure:
            simulatedPressure(std::hypot(s.x-x,s.y-y),double(s.timestampNs-stamp)/1e9,pressure);
        tail->knots.push_back({s.x,s.y,static_cast<std::uint16_t>(std::lround(pressure*65535))});
        if(s.timestampNs>stamp){x=s.x;y=s.y;stamp=s.timestampNs;}
    }
    metrics_.generatedKnots+=tail->knots.size();preview_=std::move(tail);
    return true;
}
double CompactBrushSession::latestPressure()const noexcept{return confirmedCount_?double(lastKnot_.pressure)/65535:.5;}
std::vector<foundation::InkKnot> CompactBrushSession::knots()const {
    std::vector<const BrushPreviewDelta::Chunk*> chunks;
    for(auto c=chunks_.get();c;c=c->previous.get())chunks.push_back(c);
    std::vector<foundation::InkKnot> result;
    for(auto it=chunks.rbegin();it!=chunks.rend();++it)for(const auto& k:(*it)->knots)
        if(result.empty()||result.back()!=k)result.push_back(k);
    auto pending=pending_;
    if(config_.algorithmVersion==2&&confirmedCount_&&pending.back()!=lastKnot_)pending.push_back(lastKnot_);
    for(const auto& k:simplifyInkKnots(pending,config_.geometryEpsilon))if(result.empty()||result.back()!=k)result.push_back(k);
    return result;
}
}
