#pragma once
#include "canvas/runtime/ink_brush.hpp"
#include "canvas/ink/transient_trail_geometry.hpp"
#include <algorithm>
#include <deque>
#include <unordered_map>

namespace canvas::ink {
// One bounded temporal owner shared by laser and object-eraser presentation.
// InkBrushConfig supplies captured paint/width/lifetime only; nothing is committed.
class TransientTrails final {
public:
    struct Point final {double x,y;std::uint64_t time;};
    struct Trail final {foundation::InkBrushConfig config;std::deque<Point> points;bool contactActive=true;};
    bool begin(std::uint64_t id,const foundation::InkBrushConfig& config) {
        if(!id||trails_.contains(id)||trails_.size()>=16||!foundation::validInkBrushConfig(config)||config.family!=foundation::InkFamily::kLaser)return false;
        trails_.emplace(id,Trail{config,{}});return true;
    }
    bool append(std::uint64_t id,double x,double y,std::uint64_t time) {
        const auto it=trails_.find(id);
        if(it==trails_.end()||!std::isfinite(x)||!std::isfinite(y)||!it->second.contactActive)return false;
        auto& t=it->second;
        if(!t.points.empty()&&time<t.points.back().time)return false;
        t.points.push_back({x,y,time});
        double length=0;std::size_t keep=t.points.size();
        for(std::size_t i=t.points.size()-1;i>0;--i){length+=std::hypot(t.points[i].x-t.points[i-1].x,t.points[i].y-t.points[i-1].y);if(length>t.config.laserTailLength){keep=t.points.size()-i+1;break;}}
        while(t.points.size()>std::min<std::size_t>(1024,keep))t.points.pop_front();
        return true;
    }
    bool end(std::uint64_t id) {
        const auto it=trails_.find(id);if(it==trails_.end())return false;
        it->second.contactActive=false;return true;
    }
    bool tick(std::uint64_t time) {
        const bool had=activeCount()!=0;
        for(auto it=trails_.begin();it!=trails_.end();){auto& t=it->second;
            const auto fade=std::uint64_t(t.config.laserFadeMs)*1'000'000;
            const auto cutoff=time>fade?time-fade:0;
            // Keep one boundary point for continuous interpolation as the old
            // end retreats. An idle held contact survives with no visible points.
            while(t.points.size()>1&&t.points[1].time<=cutoff)t.points.pop_front();
            if(!t.points.empty()&&time>=t.points.back().time&&time-t.points.back().time>=fade)t.points.clear();
            if(t.points.empty()&&!t.contactActive)it=trails_.erase(it);else ++it;
        }return had;
    }
    FadingTrailPreview sample(std::uint64_t id,std::uint64_t time)const {
        FadingTrailPreview out;const auto it=trails_.find(id);if(it==trails_.end())return out;
        const auto& t=it->second;const auto fade=std::uint64_t(t.config.laserFadeMs)*1'000'000;
        if(t.points.empty()||!fade)return out;
        const auto cutoff=time>fade?time-fade:0;
        out.points.reserve(t.points.size());double distance=0;
        for(std::size_t i=t.points.size();i>0;--i){auto p=t.points[i-1];
            if(i<t.points.size())distance+=std::hypot(p.x-t.points[i].x,p.y-t.points[i].y);
            if(p.time<cutoff){
                if(i>=t.points.size()||t.points[i].time<=cutoff)continue;
                const auto& next=t.points[i];const double fraction=double(cutoff-p.time)/double(next.time-p.time);
                p.x+=(next.x-p.x)*fraction;p.y+=(next.y-p.y)*fraction;p.time=cutoff;
            }
            const double age=time>=p.time?double(time-p.time)/double(fade):0;
            const double life=std::clamp(1.-age,0.,1.);
            const double spatial=std::clamp(1.-distance/t.config.laserTailLength,0.,1.);
            out.points.push_back({p.x,p.y,static_cast<float>(t.config.size*std::sqrt(life*spatial)),static_cast<float>(life*life*spatial)});
        }
        std::reverse(out.points.begin(),out.points.end());return out;
    }
    bool cancel(std::uint64_t id){return trails_.erase(id)!=0;}
    void clear(){trails_.clear();}
    std::size_t activeCount()const{std::size_t n=0;for(const auto& [id,t]:trails_){(void)id;n+=!t.points.empty();}return n;}
    std::size_t retainedCount()const{std::size_t count=0;for(const auto& [id,t]:trails_){(void)id;count+=t.points.size();}return count;}
    const auto& trails()const{return trails_;}
private:std::unordered_map<std::uint64_t,Trail> trails_;
};
using LaserTrail=TransientTrails;
}
