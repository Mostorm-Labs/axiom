#include "canvas/ink/laser_trail.hpp"
#include <stdexcept>
int main(){
    canvas::ink::LaserTrail trail;
    canvas::foundation::InkBrushConfig config;config.family=canvas::foundation::InkFamily::kLaser;
    if(!trail.begin(1,config)||!trail.append(1,10,20,1'000'000)||trail.activeCount()!=1)throw std::runtime_error("laser begin");
    for(unsigned i=0;i<8192;++i)if(!trail.append(1,i*2.,20,1'000'000+i*1'000'000ULL))throw std::runtime_error("laser append");
    if(trail.retainedCount()>1024)throw std::runtime_error("unbounded laser");
    if(!trail.tick(10'000'000'000ULL)||trail.activeCount()!=0)throw std::runtime_error("laser must fade without input");
    if(!trail.append(1,90,20,11'000'000'000ULL))throw std::runtime_error("idle held contact lost");
    if(!trail.end(1)||!trail.tick(12'000'000'000ULL)||trail.trails().contains(1))throw std::runtime_error("ended contact retained");
    config.laserFadeMs=600;config.laserTailLength=1000;
    if(!trail.begin(2,config)||!trail.append(2,0,0,1'000'000'000)||!trail.append(2,80,0,1'200'000'000))throw std::runtime_error("gradient fixture");
    auto sampled=trail.sample(2,1'300'000'000);
    if(sampled.points.size()!=2||sampled.points.front().opacity>=sampled.points.back().opacity||sampled.points.front().width>=sampled.points.back().width)throw std::runtime_error("point ages not preserved");
    (void)trail.tick(1'650'000'000);sampled=trail.sample(2,1'650'000'000);
    if(sampled.points.empty()||sampled.points.front().x<=0||sampled.points.front().opacity!=0)throw std::runtime_error("tail cutoff did not retreat continuously");
}
