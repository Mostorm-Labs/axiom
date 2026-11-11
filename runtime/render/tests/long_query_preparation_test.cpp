// Independent anchored original per-local f32 stroke/left-fold path oracle.
#define main lqpExistingFixtureMain
#include "ur2d_selection_up_test.cpp"
#undef main
#include "../src/long_query_preparation_observer.hpp"
#include "../../interaction/src/partial_erase_geometry.hpp"
#include "include/pathops/SkPathOps.h"
#include "include/core/SkPathUtils.h"
#include "include/core/SkPaint.h"
#include "include/core/SkPathBuilder.h"
namespace {
SkPath originalPath(const semantic::EraseMaskGeometry& mask){
 SkPath accumulated;
 const auto& sweep=std::get<semantic::SweptCircleMask>(mask);
 for(const auto&s:sweep.segments){SkPathBuilder b;b.moveTo(float(s.p0.position.x),float(s.p0.position.y));b.cubicTo(float(s.control1.x),float(s.control1.y),float(s.control2.x),float(s.control2.y),float(s.p1.position.x),float(s.p1.position.y));SkPaint p;p.setStyle(SkPaint::kStroke_Style);p.setStrokeWidth(float(2*std::max(s.p0.radius,s.p1.radius)));p.setStrokeCap(SkPaint::kRound_Cap);p.setStrokeJoin(SkPaint::kRound_Join);const auto part=skpathutils::FillPathWithPaint(b.detach(),p);auto next=Op(accumulated,part,kUnion_SkPathOp);require(bool(next),"original sequential union failed");accumulated=*next;}
 return accumulated;
}
semantic::EraseMaskGeometry input(unsigned count,unsigned local){std::vector<foundation::WorldPoint>trace{{420,160}};for(unsigned i=1;i<=count;++i)trace.push_back({float(420+7*std::sin(i*.17)),float(160+170.*i/1600)});return *interaction::partialEraseGeometry(trace,{1,0,0,1,double(local)*.125,double(local)*.25},16);}
}
void edgeCases(){
 const auto geometry=input(64,0);const auto expected=originalPath(geometry);
 {
  auto budget=std::make_shared<resources::ResourceBudgetManager>();EraseMaskPathCache cache;
  cache.prepareQueryPrefix(geometry,budget,8*1024*1024,32);
  auto fork=geometry;std::get<semantic::SweptCircleMask>(fork).segments[31].control2.x+=.5;
  auto path=cache.get(fork);require(path&&longQueryPreparationPath(path)==originalPath(fork),"computed-tail fork falsecompletedpath");
  cache.discardQueryPrefixes();path.reset();cache.clear();require(budget->used()==0,"fork clear lease");
 }
 {
  auto budget=std::make_shared<resources::ResourceBudgetManager>();EraseMaskPathCache cache;
  cache.prepareQueryPrefix(input(32,0),budget,8*1024*1024,8);
  cache.prepareQueryPrefix(geometry,budget,8*1024*1024,8);
  auto final=cache.get(geometry);require(final&&longQueryPreparationPath(final)==expected,"partial final remainder differs original");
  cache.clear();require(budget->used()>0,"external path pin uncharged afterclear");final.reset();require(budget->used()==0,"external path last release");
 }
 {
  auto budget=std::make_shared<resources::ResourceBudgetManager>();EraseMaskPathCache cache;
  ReferenceTraversalEntry entry;entry.presentationEraseActive=true;entry.presentationEraseMasks={{{},geometry}};
  PreparationAllowance expired;expired.start-=std::chrono::milliseconds(10);
  cache.prepareQueryPrefixes({&entry,1},budget,8*1024*1024,expired);
  require(cache.work().history.queryUnionOps==0&&expired.steps==0,"expiredzero-stepframe performed extraPathOp");
  RenderPreparationBudget one;one.maxCoverageSteps=1;PreparationAllowance allowance(one);
  cache.prepareQueryPrefixes({&entry,1},budget,8*1024*1024,allowance);
  require(cache.work().history.queryUnionOps<=1&&allowance.steps<=1,"sharedone-stepbudget exceeded");
  auto bad=geometry;std::get<semantic::SweptCircleMask>(bad).segments.back().control1.x=std::numeric_limits<double>::quiet_NaN();
  const auto before=cache.work();cache.prepareQueryPrefix(bad,budget,8*1024*1024,32);require(cache.work().history.queryUnionOps==before.history.queryUnionOps,"NaN optionalgeometry didwork");
  auto overflow=geometry;auto& large=std::get<semantic::SweptCircleMask>(overflow).segments.front();large.p0.position.x=std::numeric_limits<double>::max();large.p1.position.x=large.p0.position.x;large.control1.x=large.p0.position.x;large.control2.x=large.p0.position.x;
  cache.prepareQueryPrefix(overflow,budget,8*1024*1024,32);auto rejected=cache.get(overflow);
  require(!rejected||(longQueryPreparationPath(rejected).isFinite()&&longQueryPreparationPath(rejected)==originalPath(overflow)),"finite double overflowing f32 changed original path or admitted nonfinite prefix");
  rejected.reset();
  cache.discardQueryPrefixes();cache.clear();require(budget->used()==0,"cancel partialjobs leaked");
 }
 {
  auto budget=std::make_shared<resources::ResourceBudgetManager>(resources::BudgetLimits{1024,1024});EraseMaskPathCache cache;
  cache.prepareQueryPrefix(geometry,budget,8*1024*1024,32);require(cache.work().history.queryUnionOps==0,"unprecharged globaldenial constructedpath");cache.clear();require(budget->used()==0,"denied prep lease");
 }
}
int main(int argc,char**argv){try{
 require(argc==2,"output.json");edgeCases();auto budget=std::make_shared<resources::ResourceBudgetManager>(resources::BudgetLimits{256U*1024U*1024U,512U*1024U*1024U});EraseMaskPathCache cache;
 for(unsigned n=8;n<=1600;n+=8)for(unsigned local=0;local<16;++local)cache.prepareQueryPrefix(input(n,local),budget,8*1024*1024,8);
 const auto before=cache.work();auto entry=cache.get(input(1600,0));require(bool(entry),"final query path");const auto after=cache.work();const auto path=longQueryPreparationPath(entry);const auto original=originalPath(input(1600,0));require(path==original,"prefix path changed original f32 sequential PathOps");
 const auto remaining=after.history.queryUnionOps-before.history.queryUnionOps;
 json result{{"status",remaining?"RED":"GREEN"},{"prepared_segments",before.history.queryUnionOps},{"final_remainder_unions",remaining},{"path_equal",true},{"resident",after.bytes}};std::ofstream(argv[1])<<result.dump(2)<<'\n';std::cout<<result.dump()<<'\n';entry.reset();cache.clear();require(budget->used()==0,"prefix lease leak");
 require(remaining==0,"ELIGIBLE_STABLE_PREFIX_STALLED_BEFORE_FINAL_UP");return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
