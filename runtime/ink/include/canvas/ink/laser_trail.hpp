#pragma once
#include "canvas/runtime/ink_brush.hpp"
#include <algorithm>
#include <deque>
#include <unordered_map>

namespace canvas::ink {
class LaserTrail final {
public:
    struct Point final {double x,y;std::uint64_t time;};
    struct Trail final {foundation::InkBrushConfig config;std::deque<Point> points;};
    bool begin(std::uint64_t id,const foundation::InkBrushConfig& config) {
        if(!id||trails_.contains(id)||trails_.size()>=16||!foundation::validInkBrushConfig(config)||config.family!=foundation::InkFamily::kLaser)return false;
        trails_.emplace(id,Trail{config,{}});return true;
    }
    bool append(std::uint64_t id,double x,double y,std::uint64_t time) {
        const auto it=trails_.find(id);
        if(it==trails_.end()||!std::isfinite(x)||!std::isfinite(y))return false;
        auto& t=it->second;
        if(!t.points.empty()&&time<t.points.back().time)return false;
        t.points.push_back({x,y,time});
        double length=0;std::size_t keep=t.points.size();
        for(std::size_t i=t.points.size()-1;i>0;--i){length+=std::hypot(t.points[i].x-t.points[i-1].x,t.points[i].y-t.points[i-1].y);if(length>t.config.laserTailLength){keep=t.points.size()-i;break;}}
        while(t.points.size()>std::min<std::size_t>(1024,keep))t.points.pop_front();
        return true;
    }
    bool tick(std::uint64_t time) {
        const bool had=!trails_.empty();
        for(auto it=trails_.begin();it!=trails_.end();){auto& t=it->second;
            const auto fade=std::uint64_t(t.config.laserFadeMs)*1'000'000;
            while(!t.points.empty()&&time>=t.points.front().time&&time-t.points.front().time>=fade)t.points.pop_front();
            if(t.points.empty())it=trails_.erase(it);else ++it;
        }return had;
    }
    bool cancel(std::uint64_t id){return trails_.erase(id)!=0;}
    void clear(){trails_.clear();}
    std::size_t activeCount()const{return trails_.size();}
    std::size_t retainedCount()const{std::size_t count=0;for(const auto& [id,t]:trails_){(void)id;count+=t.points.size();}return count;}
    const auto& trails()const{return trails_;}
private:std::unordered_map<std::uint64_t,Trail> trails_;
};
}
