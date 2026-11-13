#include "canvas/ink/compact_contour.hpp"
#include <cmath>
#include <cstdio>
#include <bit>
void write(double value){std::printf("%016llx",(unsigned long long)std::bit_cast<unsigned long long>(value));}
int main(){
 canvas::foundation::InkBrushConfig c;c.version=2;c.algorithmVersion=4;c.signalVersion=3;c.smoothing=.61;
 for(unsigned fixture=0;fixture<2;++fixture){std::vector<canvas::foundation::InkKnot> knots;
  for(unsigned i=0;i<500;++i)knots.push_back({i*2.,fixture?100+30*std::sin(i*.015):100.,std::uint16_t(12000+(i%200)*200)});
  const auto contour=canvas::ink::compileCompactContour(c,knots);std::printf("%u %zu %zu\n",fixture,contour.patches.size(),contour.caps.size());
  for(auto p:contour.patches)for(auto v:{p.leftStart,p.leftControl,p.leftEnd,p.rightStart,p.rightControl,p.rightEnd}){write(v.x);write(v.y);std::puts("");}
  for(auto cap:contour.caps){write(cap.x);write(cap.y);write(cap.radius);std::puts("");}
 }
}
