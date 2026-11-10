#include "canvas/render/selection_region_geometry.hpp"
#include "canvas/ink/brush_package_catalog.hpp"
#include <chrono>
#include <cmath>
#include <iostream>
#include <array>
using namespace canvas;
int main(){
 for(unsigned knots:{512U,2048U,7199U})for(unsigned histories:{0U,2U}){
  RuntimeSceneRecord r;r.objectId=ObjectId::fromUint64(1);r.kind=semantic::ObjectKind::kVectorStroke;r.kindVersion=3;
  semantic::CompactStrokeContent c;c.stroke.config=*ink::BrushPackageCatalog{}.loadDefault("round-handdrawn",4).package.compactConfig;c.stroke.config.size=16;c.stroke.seed=42;
  for(unsigned i=0;i<knots;++i)c.stroke.knots.push_back({250+180*std::sin(i*.037),220+150*std::cos(i*.053),static_cast<std::uint16_t>(20000+(i%100)*300)});
  r.content=std::move(c);
  for(unsigned h=0;h<histories;++h){semantic::SweptCircleMask sweep;
   for(unsigned i=1;i<=80;++i){semantic::Vec2 a{240+70*std::sin((i-1)*.17+h*.1),210+65*std::cos((i-1)*.11)},b{240+70*std::sin(i*.17+h*.1),210+65*std::cos(i*.11)};sweep.segments.push_back({{a,15},{b,15},{a.x+(b.x-a.x)/3,a.y+(b.y-a.y)/3},{a.x+2*(b.x-a.x)/3,a.y+2*(b.y-a.y)/3}});}
   r.eraseMasks.push_back({ObjectId::fromUint64(h+2),std::move(sweep)});
  }
  const foundation::WorldPoint region[]{{230,200},{235,200},{235,205},{230,205}};
  std::cout<<"{\"stage\":\"selection-start\",\"knots\":"<<knots<<",\"historical_masks\":"<<histories<<"}"<<std::endl;
  auto t=std::chrono::steady_clock::now();const auto hit=render::intersectVisibleSelectionGeometry(r,region);auto ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-t).count();
  std::cout<<"{\"stage\":\"selection-result\",\"knots\":"<<knots<<",\"historical_masks\":"<<histories<<",\"query_ms\":"<<ms<<",\"result\":"<<static_cast<unsigned>(hit)<<"}"<<std::endl;
  semantic::FilledPathMask demand;demand.path.commands={semantic::MoveTo{{230,200}},semantic::LineTo{{235,200}},semantic::LineTo{{235,205}},semantic::LineTo{{230,205}},semantic::ClosePath{}};
  t=std::chrono::steady_clock::now();const auto erase=render::intersectEraseGeometry(r,demand);ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-t).count();
  std::cout<<"{\"stage\":\"erase-result\",\"knots\":"<<knots<<",\"historical_masks\":"<<histories<<",\"query_ms\":"<<ms<<",\"result\":"<<static_cast<unsigned>(erase)<<"}"<<std::endl;
 }
}
