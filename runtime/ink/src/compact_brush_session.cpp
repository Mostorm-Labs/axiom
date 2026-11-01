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
    if(config_.algorithmVersion==3){
        if(distance<=1e-6)return pressure;
        if(config_.pressureModel==foundation::InkPressureModel::kDistance){
            const auto sp=std::min(1.,distance/config_.size);
            return std::clamp(pressure+((1-sp)-pressure)*(sp*.275),0.,1.);
        }
    }
    if(dt<=0)return pressure;
    const auto t=std::clamp((distance/dt-config_.vLow)/(config_.vHigh-config_.vLow),0.,1.);
    const auto target=config_.pMax-(config_.pMax-config_.pMin)*t*t*(3-2*t);
    return std::clamp(pressure+(target-pressure)*(1-std::exp(-dt/config_.tauSeconds)),config_.pMin,config_.pMax);
}
bool CompactBrushSession::append(std::span<const TimedBrushSample> confirmed,std::span<const TimedBrushSample> predicted)noexcept {
    if(config_.algorithmVersion==4||config_.algorithmVersion==5)return appendQuality(confirmed,predicted);
    auto sequence=lastSequence_,time=lastTimestamp_;
    for(const auto& s:confirmed){if(!validSample(s,sequence)||(sequence&&s.timestampNs<time))return false;sequence=s.sequence;time=s.timestampNs;}
    for(const auto& s:predicted){if(!validSample(s,sequence)||(sequence&&s.timestampNs<time))return false;sequence=s.sequence;time=s.timestampNs;}
    if(confirmedCount_>262144||confirmed.size()>262144-confirmedCount_||predicted.size()>1024)return false;
    // All input admission precedes retained state changes. Prefix chunks are
    // immutable; only the bounded pending window is copied for publication.
    const auto length=[](std::span<const foundation::InkKnot> knots){double total=0;
        for(std::size_t i=1;i<knots.size();++i)total+=std::hypot(knots[i].x-knots[i-1].x,knots[i].y-knots[i-1].y);
        return total;
    };
    const auto retain=[&](const foundation::InkKnot& k){
        if(config_.algorithmVersion==3&&!pending_.empty()&&pending_.back()==k)return;
        if(!pending_.empty())pendingLength_+=std::hypot(k.x-pending_.back().x,k.y-pending_.back().y);
        pending_.push_back(k);
        if(pending_.size()>=64||pendingLength_>=32){
            std::size_t cut=pending_.size()-1;
            if(config_.algorithmVersion==3){
                // End taper can retract coverage. Only freeze geometry safely
                // behind its distance horizon; retain one connecting knot.
                double remaining=0;cut=0;
                for(std::size_t i=pending_.size()-1;i>0;--i){
                    remaining+=std::hypot(pending_[i].x-pending_[i-1].x,pending_[i].y-pending_[i-1].y);
                    if(remaining>=config_.endTaper+config_.size*2+32){cut=i-1;break;}
                }
                if(!cut){
                    if(pending_.size()>=256){pending_=simplifyInkKnots(pending_,config_.geometryEpsilon,&metrics_.simplificationVisits);pendingLength_=length(pending_);}
                    return;
                }
            }
            auto chunk=std::make_shared<BrushPreviewDelta::Chunk>();chunk->previous=chunks_;
            chunk->startDistance=prefixLength_;
            chunk->knots=simplifyInkKnots({pending_.data(),cut+1},config_.geometryEpsilon,&metrics_.simplificationVisits);
            prefixLength_+=length(chunk->knots);chunk->totalDistance=prefixLength_;
            metrics_.generatedKnots+=chunk->knots.size();chunks_=std::move(chunk);
            pending_.erase(pending_.begin(),pending_.begin()+cut);pendingLength_=length(pending_);
        }
    };
    for(const auto& s:confirmed){
        double x=s.x,y=s.y;
        if(config_.algorithmVersion==3){
            const auto t=.15+.85*(1-config_.streamline);
            if(!confirmedCount_){filteredX_=x;filteredY_=y;}
            else {filteredX_+=(x-filteredX_)*t;filteredY_+=(y-filteredY_)*t;}
            x=s.terminal?s.x:filteredX_;y=s.terminal?s.y:filteredY_;
        }
        auto p=config_.pressureSource==BrushPressureSource::kDevice?s.pressure:previousPressure_;
        if(lastSequence_&&config_.pressureSource==BrushPressureSource::kSimulated){
            const auto distance=config_.algorithmVersion==3&&config_.pressureModel==foundation::InkPressureModel::kDistance
                ?std::hypot(x-lastKnot_.x,y-lastKnot_.y):std::hypot(s.x-previousX_,s.y-previousY_);
            // A stationary release may complete the lagging filtered endpoint,
            // but that catch-up is not new pressure input.
            if(config_.algorithmVersion!=3||std::hypot(s.x-previousX_,s.y-previousY_)>1e-6)
                p=simulatedPressure(distance,double(s.timestampNs-previousTime_)/1e9,previousPressure_);
        }
        if(config_.algorithmVersion==3&&s.terminal&&config_.pressureSource==BrushPressureSource::kSimulated)
            p=std::min(p,previousPressure_);
        const foundation::InkKnot k{x==0?0:x,y==0?0:y,static_cast<std::uint16_t>(std::lround(p*65535))};
        if(config_.algorithmVersion>=2){
            if(!confirmedCount_){retain(k);curveStart_=k;}
            else if(confirmedCount_>=2){
                if(confirmedCount_==2){
                    curveStart_=inkMidpoint(previousControl_,lastKnot_);
                    if(config_.algorithmVersion==3){
                        curveStart_.x=lastKnot_.x+(curveStart_.x-lastKnot_.x)*config_.smoothing;
                        curveStart_.y=lastKnot_.y+(curveStart_.y-lastKnot_.y)*config_.smoothing;
                        curveStart_.pressure=static_cast<std::uint16_t>(std::lround(lastKnot_.pressure+(double(curveStart_.pressure)-lastKnot_.pressure)*config_.smoothing));
                    }
                    retain(curveStart_);
                }
                auto end=inkMidpoint(lastKnot_,k);
                auto control=lastKnot_;
                if(config_.algorithmVersion==3){
                    const auto smooth=config_.smoothing;
                    end.x=lastKnot_.x+(end.x-lastKnot_.x)*smooth;
                    end.y=lastKnot_.y+(end.y-lastKnot_.y)*smooth;
                    end.pressure=static_cast<std::uint16_t>(std::lround(lastKnot_.pressure+(double(end.pressure)-lastKnot_.pressure)*smooth));
                    const auto mid=inkMidpoint(curveStart_,end);
                    control.x=mid.x+(control.x-mid.x)*smooth;
                    control.y=mid.y+(control.y-mid.y)*smooth;
                    control.pressure=static_cast<std::uint16_t>(std::lround(mid.pressure+(double(control.pressure)-mid.pressure)*smooth));
                }
                std::vector<foundation::InkKnot> segment;
                appendInkQuadratic(segment,curveStart_,control,end);
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
    auto tail=std::make_shared<BrushPreviewDelta::Chunk>();tail->previous=chunks_;tail->startDistance=prefixLength_;
    auto pending=pending_;
    if(config_.algorithmVersion>=2&&confirmedCount_&&pending.back()!=lastKnot_)pending.push_back(lastKnot_);
    tail->knots=simplifyInkKnots(pending,config_.geometryEpsilon,&metrics_.simplificationVisits);metrics_.copiedKnots+=pending.size();
    tail->totalDistance=prefixLength_+length(tail->knots);
    if(config_.algorithmVersion>=2&&!predicted.empty()){
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
double CompactBrushSession::latestPressure()const noexcept{if((config_.algorithmVersion==4||config_.algorithmVersion==5)&&qualityConfirmed_&&!qualityConfirmed_->knots.empty())return double(qualityConfirmed_->knots.back().pressure)/65535;
    return confirmedCount_?double(lastKnot_.pressure)/65535:.5;}
std::vector<foundation::InkKnot> CompactBrushSession::knots()const {
    if(config_.algorithmVersion==4||config_.algorithmVersion==5){
        std::vector<const BrushPreviewDelta::Chunk*> pieces;
        for(auto p=qualityConfirmed_.get();p;p=p->previous.get())pieces.push_back(p);
        std::vector<foundation::InkKnot> result;
        for(auto i=pieces.rbegin();i!=pieces.rend();++i)for(auto k:(*i)->knots)if(result.empty()||result.back()!=k)result.push_back(k);
        return result;
    }
    std::vector<const BrushPreviewDelta::Chunk*> chunks;
    for(auto c=chunks_.get();c;c=c->previous.get())chunks.push_back(c);
    std::vector<foundation::InkKnot> result;
    for(auto it=chunks.rbegin();it!=chunks.rend();++it)for(const auto& k:(*it)->knots)
        if(result.empty()||result.back()!=k)result.push_back(k);
    auto pending=pending_;
    if(config_.algorithmVersion>=2&&confirmedCount_&&pending.back()!=lastKnot_)pending.push_back(lastKnot_);
    for(const auto& k:simplifyInkKnots(pending,config_.geometryEpsilon))if(result.empty()||result.back()!=k)result.push_back(k);
    return result;
}
}
