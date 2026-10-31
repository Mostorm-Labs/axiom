#include "canvas/ink/laser_trail.hpp"
#include <stdexcept>
int main(){
    canvas::ink::LaserTrail trail;
    canvas::foundation::InkBrushConfig config;config.family=canvas::foundation::InkFamily::kLaser;
    if(!trail.begin(1,config)||!trail.append(1,10,20,1'000'000)||trail.activeCount()!=1)throw std::runtime_error("laser begin");
    for(unsigned i=0;i<8192;++i)if(!trail.append(1,i*2.,20,1'000'000+i*1'000'000ULL))throw std::runtime_error("laser append");
    if(trail.retainedCount()>1024)throw std::runtime_error("unbounded laser");
    if(!trail.tick(10'000'000'000ULL)||trail.activeCount()!=0)throw std::runtime_error("laser must fade without input");
}
