#include "canvas/ink/compact_brush_session.hpp"
#include "canvas/ink/compact_geometry.hpp"
#include <cmath>
namespace canvas::ink {
namespace {
double pressureStep(double p,double distance,double size){const auto sp=std::min(1.,distance/size);return std::min(1.,p+((1-sp)-p)*(sp*.275));}
double arc(std::span<const foundation::InkKnot> knots){return compactLength(knots);}
}
void CompactBrushSession::retainQuality(foundation::InkKnot k){
    if(!pending_.empty()&&pending_.back()==k)return;
    if(!pending_.empty())pendingLength_+=std::hypot(k.x-pending_.back().x,k.y-pending_.back().y);
    if(pending_.size()>1&&std::hypot(k.x-pending_[pending_.size()-2].x,k.y-pending_[pending_.size()-2].y)<.25&&
       std::abs(int(k.pressure)-int(pending_[pending_.size()-2].pressure))<655){
        const auto& prior=pending_[pending_.size()-2];
        pendingLength_+=std::hypot(k.x-prior.x,k.y-prior.y)-std::hypot(pending_.back().x-prior.x,pending_.back().y-prior.y)-std::hypot(k.x-pending_.back().x,k.y-pending_.back().y);
        pending_.back()=k;
    }else pending_.push_back(k);
    // Keep enough replaceable distance for taper and neighbor/anchor smoothing.
    const auto horizon=config_.endTaper+config_.size*4+32;
    if(pendingLength_<horizon+32||pending_.size()<4)return;
    double remaining=0;std::size_t cut=0;
    for(std::size_t i=pending_.size()-1;i>0;--i){remaining+=std::hypot(pending_[i].x-pending_[i-1].x,pending_[i].y-pending_[i-1].y);if(remaining>=horizon){cut=i-1;break;}}
    if(!cut)return;
    auto chunk=std::make_shared<BrushPreviewDelta::Chunk>();chunk->previous=chunks_;chunk->startDistance=prefixLength_;
    chunk->knots.assign(pending_.begin(),pending_.begin()+cut+1);
    prefixLength_+=arc(chunk->knots);chunk->totalDistance=prefixLength_;
    if(config_.algorithmVersion==4)qualityContour_->append(chunk->knots,prefixLength_+remaining,chunk->contour);
    metrics_.generatedKnots+=chunk->knots.size();chunks_=std::move(chunk);
    pending_.erase(pending_.begin(),pending_.begin()+cut);pendingLength_=arc(pending_);
}
void CompactBrushSession::acceptQualityPoint(const reference::StrokePoint& q){
    double p=q.pressure;
    if(config_.pressureSource==BrushPressureSource::kSimulated){
        if(config_.pressureModel==foundation::InkPressureModel::kDistance){
            qualityPressure_=pressureStep(qualityPressure_,q.distance,config_.size);p=qualityPressure_;
        }
    }
    retainQuality({q.x,q.y,static_cast<std::uint16_t>(std::lround(std::clamp(p,0.,1.)*65535))});
}
void CompactBrushSession::publishQuality(bool terminal){
    auto compiler=*qualityContour_;
    auto tail=std::make_shared<BrushPreviewDelta::Chunk>();tail->previous=chunks_;tail->startDistance=prefixLength_;tail->knots=pending_;
    // The pending points are end-noise candidates. The final observation is
    // always represented; no duplicate zero-distance up creates a pressure step.
    if(!qualityPending_.empty()){
        const auto& q=qualityPending_.back();
        auto p=config_.pressureSource==BrushPressureSource::kDevice||config_.pressureModel==foundation::InkPressureModel::kTimedSpeed?q.pressure:
            config_.pressureModel==foundation::InkPressureModel::kDistance?pressureStep(qualityPressure_,q.distance,config_.size):qualityPressure_;
        foundation::InkKnot k{q.x,q.y,static_cast<std::uint16_t>(std::lround(std::clamp(p,0.,1.)*65535))};
        if(tail->knots.empty()||tail->knots.back()!=k)tail->knots.push_back(k);
    }
    if(tail->knots.empty())return;
    tail->totalDistance=prefixLength_+arc(tail->knots);
    if(config_.algorithmVersion==4){compiler.append(tail->knots,tail->totalDistance,tail->contour);compiler.finish(tail->totalDistance,tail->contour);}
    metrics_.copiedKnots+=tail->knots.size();metrics_.generatedKnots+=tail->knots.size();
    preview_=qualityConfirmed_=std::move(tail);(void)terminal;
}
bool CompactBrushSession::appendQuality(std::span<const TimedBrushSample> confirmed,std::span<const TimedBrushSample> predicted){
    auto sequence=lastSequence_,stamp=lastTimestamp_;
    for(const auto& s:confirmed){if(!validSample(s,sequence)||(sequence&&s.timestampNs<stamp))return false;sequence=s.sequence;stamp=s.timestampNs;}
    for(const auto& s:predicted){if(!validSample(s,sequence)||(sequence&&s.timestampNs<stamp))return false;sequence=s.sequence;stamp=s.timestampNs;}
    if(confirmed.size()>262144-confirmedCount_||predicted.size()>1024)return false;
    if(!qualityContour_){auto geometry=config_.geometryConfig.value_or(foundation::InkBrushConfig{});geometry.algorithmVersion=4;geometry.thinning=config_.geometryConfig?geometry.thinning:.99;qualityContour_.emplace(geometry);}
    bool terminal=false;
    for(const auto& s:confirmed){
        terminal=s.terminal;++confirmedCount_;++metrics_.samples;
        const auto rawDistance=lastSequence_?std::hypot(s.x-previousX_,s.y-previousY_):0;
        if(config_.pressureModel==foundation::InkPressureModel::kTimedSpeed&&lastSequence_&&rawDistance>1e-6)
            qualityPressure_=simulatedPressure(rawDistance,double(s.timestampNs-previousTime_)/1e9,qualityPressure_);
        const bool stationary=lastSequence_&&rawDistance<=1e-6;
        if(!qualityInitialized_){
            if(!stationary){auto captured=s;
                if(config_.pressureSource==BrushPressureSource::kSimulated&&config_.pressureModel==foundation::InkPressureModel::kTimedSpeed)captured.pressure=qualityPressure_;
                qualityStartup_.push_back(captured);}
            std::vector<reference::VectorStrokeInput> raw;raw.reserve(qualityStartup_.size());
            for(const auto& a:qualityStartup_)raw.push_back({a.x,a.y,config_.pressureSource==BrushPressureSource::kDevice||config_.pressureModel==foundation::InkPressureModel::kTimedSpeed?a.pressure:.5});
            reference::StrokeOptions options;options.size=config_.size;options.streamline=config_.streamline;options.last=terminal;
            auto points=reference::getStrokePoints(raw,options);
            // PF synthesizes positions for one/two-point inputs. A tap stays a
            // tap in the compact carrier; explicit pressure retains the actual
            // endpoint values instead of the oracle's missing-pressure .5.
            if(raw.size()==1)points={{raw.front().x,raw.front().y,raw.front().pressure,0,0,0,0}};
            if(raw.size()==2&&(config_.pressureSource==BrushPressureSource::kDevice||config_.pressureModel==foundation::InkPressureModel::kTimedSpeed)){
                const auto dx=raw.back().x-raw.front().x,dy=raw.back().y-raw.front().y,den=dx*dx+dy*dy;
                for(auto& q:points){const auto t=den>0?std::clamp(((q.x-raw.front().x)*dx+(q.y-raw.front().y)*dy)/den,0.,1.):0.;
                    q.pressure=std::lerp(raw.front().pressure,raw.back().pressure,t);}
            }
            // Before ten normalized points the entire short preview is bounded
            // and replaceable; after that initialization never changes.
            const bool freeze=points.size()>=11||terminal||qualityStartup_.size()>=128;
            if(!points.empty()){
                if(config_.pressureModel==foundation::InkPressureModel::kDistance)qualityPressure_=.5;
                if(config_.pressureSource==BrushPressureSource::kSimulated&&config_.pressureModel==foundation::InkPressureModel::kDistance)
                    for(std::size_t i=0;i<std::min<std::size_t>(10,points.size());++i)qualityPressure_=(qualityPressure_+pressureStep(qualityPressure_,points[i].distance,config_.size))*.5;
                if(freeze){
                    qualityInitialized_=true;qualityStartup_.clear();qualityPending_.clear();pending_.clear();pendingLength_=0;
                    for(const auto& q:points)qualityPending_.push_back(q);
                    qualityLast_=points.back();qualityRunning_=qualityLast_.running_length;
                }else{
                    // Short-history oracle is capped at startup; publish on a
                    // disposable compiler, never mutate the stable signal state.
                    pending_.clear();qualityPending_.clear();pendingLength_=0;
                    auto p=qualityPressure_;
                    for(std::size_t i=0;i<points.size();++i){const auto& q=points[i];
                        if(i+1<points.size()&&points.back().running_length-q.running_length<3)continue;
                        if(config_.pressureSource==BrushPressureSource::kSimulated&&config_.pressureModel==foundation::InkPressureModel::kDistance)p=pressureStep(p,q.distance,config_.size);
                        pending_.push_back({q.x,q.y,static_cast<std::uint16_t>(std::lround((config_.pressureSource==BrushPressureSource::kDevice||config_.pressureModel==foundation::InkPressureModel::kTimedSpeed?q.pressure:p)*65535))});
                    }
                    pendingLength_=arc(pending_);
                }
            }
        }else if(!stationary){
            const auto t=.15+.85*(1-config_.streamline);
            const auto x=terminal?s.x:qualityLast_.x+(s.x-qualityLast_.x)*t;
            const auto y=terminal?s.y:qualityLast_.y+(s.y-qualityLast_.y)*t;
            const auto d=std::hypot(x-qualityLast_.x,y-qualityLast_.y);
            if(d>1e-9){qualityRunning_+=d;qualityLast_={x,y,config_.pressureSource==BrushPressureSource::kDevice?s.pressure:config_.pressureModel==foundation::InkPressureModel::kTimedSpeed?qualityPressure_:.5,(qualityLast_.x-x)/d,(qualityLast_.y-y)/d,d,qualityRunning_};qualityPending_.push_back(qualityLast_);}
        }
        if(qualityInitialized_){
            while(qualityPending_.size()>1&&qualityPending_.back().running_length-qualityPending_.front().running_length>=3){
                acceptQualityPoint(qualityPending_.front());qualityPending_.erase(qualityPending_.begin());
            }
        }
        previousX_=s.x;previousY_=s.y;previousTime_=s.timestampNs;lastSequence_=s.sequence;lastTimestamp_=s.timestampNs;
    }
    if(confirmedCount_)publishQuality(terminal);
    if(!predicted.empty()){
        auto fork=*this;if(!fork.appendQuality(predicted,{}))return false;
        preview_=fork.preview_;
    }
    return true;
}
}
