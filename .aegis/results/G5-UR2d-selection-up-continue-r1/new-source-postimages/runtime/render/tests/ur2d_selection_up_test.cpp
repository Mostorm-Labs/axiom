#include "canvas/render/skia_renderer.hpp"
#include "canvas/ink/brush_package_catalog.hpp"
#include "../src/selection_region_geometry_internal.hpp"
#include "../src/skia_prepared_geometry.hpp"
#include <nlohmann/json.hpp>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <chrono>
#include <cmath>
#include <limits>
using namespace canvas;
using namespace canvas::render;
using namespace canvas::render::internal;
using json=nlohmann::json;
namespace {
RuntimeSceneRecord source(unsigned knots,unsigned histories){
 RuntimeSceneRecord r;r.objectId=ObjectId::fromUint64(1);r.kind=semantic::ObjectKind::kVectorStroke;r.kindVersion=3;
 semantic::CompactStrokeContent c;c.stroke.config=*ink::BrushPackageCatalog{}.loadDefault("round-handdrawn",4).package.compactConfig;c.stroke.config.size=16;c.stroke.seed=42;
 for(unsigned i=0;i<knots;++i)c.stroke.knots.push_back({250+180*std::sin(i*.037),220+150*std::cos(i*.053),std::uint16_t(20000+(i%100)*300)});
 r.content=std::move(c);
 for(unsigned h=0;h<histories;++h){semantic::SweptCircleMask sweep;
  for(unsigned i=1;i<=80;++i){semantic::Vec2 a{240+70*std::sin((i-1)*.17+h*.1),210+65*std::cos((i-1)*.11)},b{240+70*std::sin(i*.17+h*.1),210+65*std::cos(i*.11)};
   sweep.segments.push_back({{a,15},{b,15},{a.x+(b.x-a.x)/3,a.y+(b.y-a.y)/3},{a.x+2*(b.x-a.x)/3,a.y+2*(b.y-a.y)/3}});}
  r.eraseMasks.push_back({ObjectId::fromUint64(h+2),std::move(sweep)});
 }return r;
}
std::vector<foundation::WorldPoint> region(unsigned n){
 switch(n%5){
 case 0:return{{230,200},{235,200},{235,205},{230,205}};
 case 1:return{{60,60},{450,60},{450,380},{60,380}};
 case 2:return{{60,60},{450,380},{60,380},{450,60}};
 case 3:return{{250,65},{250.02F,65},{250.02F,65.02F},{250,65.02F}};
 default:return{{-1000,-1000},{-900,-1000},{-900,-900},{-1000,-900}};
 }
}
void require(bool v,const char* m){if(!v)throw std::runtime_error(m);}
SelectionGeometryHit selection(bool candidate,const RuntimeSceneRecord& record,std::span<const foundation::WorldPoint> points,
 EraseMaskPathCache& cache,std::shared_ptr<resources::ResourceBudgetManager> budget,PreparedGeometryCache& bodies,const RenderCacheOptions& options){
#if defined(UR2D_OLD_ORACLE)
 (void)candidate;(void)cache;(void)budget;(void)bodies;(void)options;
 return intersectVisibleSelectionGeometry(record,points);
#else
 return candidate?cache.selectionQuery(record,points,budget,8*1024*1024,&bodies,&options):intersectVisibleSelectionGeometry(record,points);
#endif
}
}
int main(int argc,char** argv){try{
 const bool candidate=argc>1&&std::string_view(argv[1])=="--candidate";
 const auto out=std::filesystem::path(argc>2?argv[2]:"out/ur2d-selection");std::filesystem::create_directories(out);
 json rows=json::array();
 auto budget=std::make_shared<resources::ResourceBudgetManager>();PreparedGeometryCache bodies;EraseMaskPathCache cache;RenderCacheOptions options;
 for(unsigned knots:{512U,2048U,7199U})for(unsigned histories:{0U,2U,5U,10U}){
  if(knots!=512&&histories>2)continue;
  auto r=source(knots,histories);
  for(unsigned query=0;query<5;++query){auto points=region(query);auto start=std::chrono::steady_clock::now();
   const auto hit=selection(candidate,r,points,cache,budget,bodies,options);
   rows.push_back({{"knots",knots},{"histories",histories},{"query",query},{"decision",unsigned(hit)},{"us",std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-start).count()}});
  }
 }
 for(unsigned test=0;test<10;++test){auto r=source(512,2);auto points=region(test);
  if(test==0)r.transform={0,0,0,0,1,1};
  if(test==1)points[0].x=std::numeric_limits<float>::quiet_NaN();
  if(test==2)r.content.update([](auto& v){std::get<semantic::CompactStrokeContent>(v).stroke.knots.back().x=std::numeric_limits<double>::quiet_NaN();});
  if(test==3)std::get<semantic::SweptCircleMask>(r.eraseMasks[1].geometry).segments.back().control2.x=std::numeric_limits<double>::quiet_NaN();
  if(test==4)r.content.update([](auto& v){std::get<semantic::CompactStrokeContent>(v).stroke.config.opacity=0;});
  if(test==5)r.content.update([](auto& v){std::get<semantic::CompactStrokeContent>(v).stroke.config.family=foundation::InkFamily::kChalk;});
  if(test>=6){r.transform={.8,.2,-.1,1.1,30,-15};for(auto& p:points){const auto x=p.x,y=p.y;p={float(.8*x-.1*y+30),float(.2*x+1.1*y-15)};}}
  const auto hit=selection(candidate,r,points,cache,budget,bodies,options);
  rows.push_back({{"case","negative/affine"},{"index",test},{"decision",unsigned(hit)}});
 }
 // Dot/contact/sliver around the established 2e-4 local tolerance.
 for(double x:{109.9998,110.0,110.00005,110.0002,110.01}){
  auto r=source(1,0);r.content.update([](auto& v){auto& s=std::get<semantic::CompactStrokeContent>(v).stroke;s.config.size=20;s.config.thinning=0;s.knots={{100,100,32768}};});
  std::vector<foundation::WorldPoint> p{{float(x),99},{float(x+1),99},{float(x+1),101},{float(x),101}};
  const auto hit=selection(candidate,r,p,cache,budget,bodies,options);
  rows.push_back({{"case","contact"},{"x",x},{"decision",unsigned(hit)}});
 }
 json costs=json::array();
 for(unsigned copies:{1U,8U,16U,32U}){
  auto r=source(7199,2);auto p=region(0);json samples=json::array();
  for(unsigned repeat=0;repeat<3;++repeat){const auto start=std::chrono::steady_clock::now();
   for(unsigned copy=0;copy<copies;++copy){auto alias=r;alias.objectId=ObjectId::fromUint64(copy+1);
    const auto hit=selection(candidate,alias,p,cache,budget,bodies,options);
    require(hit==SelectionGeometryHit::kMiss,"dense empty decision");}
   samples.push_back(std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count());
  }
  costs.push_back({{"copies",copies},{"ms",samples}});
  std::cout<<costs.back().dump()<<std::endl;
 }
 if(candidate){
  // A failed nonempty history admission is never empty-history identity.
  auto r=source(512,0);semantic::FilledPathMask box;
  box.path.commands={semantic::MoveTo{{60,50}},semantic::LineTo{{450,50}},semantic::LineTo{{450,390}},semantic::LineTo{{60,390}},semantic::ClosePath{}};
  require(cache.query(r,box,budget,128*1024,&bodies,&options)==SelectionGeometryHit::kHit,"memo emptyhistory positive");
  const auto emptyMemo=cache.work().equivalentQueryHits;
  require(cache.query(r,box,budget,128*1024,&bodies,&options)==SelectionGeometryHit::kHit&&
      cache.work().equivalentQueryHits==emptyMemo+1,"memo fixture did not actually retain emptyhistory result");
  auto removed=r;semantic::SweptCircleMask large;
  for(unsigned i=0;i<80;++i)large.segments.push_back({{{250,220},500},{{251,220},500},{250.3,220},{250.6,220}});
  removed.eraseMasks.push_back({ObjectId::fromUint64(80000),large});
  const auto beforeMemo=cache.work().equivalentQueryHits;
  const auto expected=intersectEraseGeometryComponentOracle(removed,box);
  require(expected==SelectionGeometryHit::kMiss&&cache.query(removed,box,budget,128*1024,&bodies,&options)==expected,"denied nonemptyhistory aliased empty memo");
  require(cache.work().equivalentQueryHits==beforeMemo,"deniedhistory incorrectly memoized");
  auto held=cache.history(removed.eraseMasks,budget,8*1024*1024);
  require(bool(held),"held history admission");
  require(cache.query(removed,box,budget,8*1024*1024,&bodies,&options)==expected,"held history recovery decision");
  cache.clear();held.reset();
 }
 auto work=cache.work();const auto prepared=bodies.statistics();cache.clear();bodies.clear();require(budget->used()==0,"selection query leases leaked");
 std::ofstream(out/"selection.json")<<json{{"candidate",candidate},{"rows",rows},{"costs",costs},{"full_validations",work.exact.fullValidations},{"compile_inputs",work.exact.compileInputs},{"prepared_builds",prepared.builds},{"prepared_compile",prepared.syncCompileInputs},{"components",work.exact.componentChecks},{"history_differences",work.history.historicalDifferences},{"lease_clear_bytes",budget->used()}}.dump(2)<<'\n';
 return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
