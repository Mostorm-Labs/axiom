#include "canvas/render/skia_renderer.hpp"
#include "canvas/ink/brush_package_catalog.hpp"
#include "canvas/semantic/snapshot.hpp"
#include "/Users/qing/.codex/worktrees/axiom-geometry-navigation/axiom/runtime/render/src/selection_region_geometry_internal.hpp"
#include "/Users/qing/.codex/worktrees/axiom-geometry-navigation/axiom/runtime/render/src/skia_prepared_geometry.hpp"
#include <iostream>
#include <fstream>
#include <filesystem>
#include <limits>
#include <cmath>
using namespace canvas;using namespace canvas::render;using namespace canvas::render::internal;
int main(){
 auto quota=std::make_shared<resources::ResourceBudgetManager>();PreparedGeometryCache bodies;EraseMaskPathCache paths;RenderCacheOptions opts;
 unsigned mismatches=0,cases=0;
 auto check=[&](RuntimeSceneRecord r,std::vector<foundation::WorldPoint> polygon){
  const auto old=intersectVisibleSelectionGeometry(r,polygon);const auto candidate=paths.selectionQuery(r,polygon,quota,8*1024*1024,&bodies,&opts);++cases;
  if(old!=candidate){++mismatches;std::cout<<"mismatch "<<cases<<" old="<<unsigned(old)<<" candidate="<<unsigned(candidate)<<std::endl;}
 };
 for(unsigned kind=0;kind<4;++kind){RuntimeSceneRecord r;r.objectId=ObjectId::fromUint64(400+kind);r.kind=semantic::ObjectKind::kVectorStroke;r.kindVersion=3;
  semantic::CompactStrokeContent c;c.stroke.config=*ink::BrushPackageCatalog{}.loadDefault("round-handdrawn",4).package.compactConfig;
  c.stroke.config.size=20;c.stroke.config.thinning=.5;c.stroke.seed=42;
  if(kind&1){c.stroke.config.family=foundation::InkFamily::kMarker;c.stroke.config.markerFlatness=.3;c.stroke.config.markerAngle=.7;}
  if(kind&2){c.stroke.config.startTaper=12;c.stroke.config.endTaper=15;}
  for(unsigned i=0;i<31;++i)c.stroke.knots.push_back({80.+30*std::sin(i*.31),80.+25*std::cos(i*.47),std::uint16_t(5000+i*1500)});r.content=c;
  for(unsigned n=0;n<40;++n){auto test=r;if(n%4){semantic::FilledPathMask mask;mask.path.commands={semantic::MoveTo{{70,65}},semantic::LineTo{{90,65}},semantic::LineTo{{90,95}},semantic::LineTo{{70,95}},semantic::ClosePath{}};test.eraseMasks.push_back({ObjectId::fromUint64(100),mask});}
   float x=45+(n*17)%75,y=45+(n*23)%75;std::vector<foundation::WorldPoint> p{{x,y},{x+8,y},{x+8,y+8},{x,y+8}};
   if(n%3==0)p={{x,y},{x+10,y+10},{x,y+10},{x+10,y}};
   if(n%7==0){test.transform={.8,.2,-.1,1.1,30,-15};for(auto& v:p){auto a=v.x,b=v.y;v={float(.8*a-.1*b+30),float(.2*a+1.1*b-15)};}}
   check(test,p);
  }
 }
 const std::filesystem::path root="/Users/qing/.codex/worktrees/axiom-geometry-navigation/axiom";
 for(unsigned n=0;n<6;++n){auto file=root/".aegis/results/G5-UR0b/control-review-ur2b-r1/finite-range-r1"/("finite-"+std::to_string(n)+".axsnap");std::ifstream f(file,std::ios::binary);std::vector<std::uint8_t> b{std::istreambuf_iterator<char>(f),{}};auto d=semantic::SnapshotCodec::decode(b,semantic::SnapshotCapability::kAllWithCompactStrokeV1);if(!d.ok())return 3;
  const auto& o=d.snapshot->objects.front();RuntimeSceneRecord r;r.objectId=o.id;r.kind=o.kind;r.kindVersion=o.kind_version;r.content=o.content;r.transform=o.transform;
  check(r,{{30,80},{70,80},{70,120},{30,120}});check(r,{{-500,-500},{-400,-500},{-400,-400},{-500,-400}});
 }
 paths.clear();bodies.clear();std::cout<<"cases="<<cases<<" mismatches="<<mismatches<<" leases="<<quota->used()<<std::endl;return mismatches?1:0;
}
