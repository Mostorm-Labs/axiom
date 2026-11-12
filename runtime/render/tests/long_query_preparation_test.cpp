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
#include <array>
namespace {
LongQueryPreparationObservation lastObservation;
bool observedPreparation=false;
void observePreparation(const LongQueryPreparationObservation& value){
 if(!value.ownerInvocation){lastObservation=value;observedPreparation=true;}
}
struct PrefixProgress {std::size_t stored=0,calls=0;std::uint64_t completed=0;json observations=json::array();};
void prepareObserved(EraseMaskPathCache& cache,const semantic::EraseMaskGeometry& geometry,
 const std::shared_ptr<resources::ResourceBudgetManager>& budget,PrefixProgress& progress){
 observedPreparation=false;
 cache.prepareQueryPrefix(geometry,budget,8*1024*1024,8);
 require(observedPreparation,"missing genuine prefix observation");
 const auto& v=lastObservation;
 require(v.completed<=8&&v.steps<=8,"one prefix opportunity exceeded eight steps");
 // Zero work with no resumed cursor can be a deadline before lookup. It
 // says nothing about stored progress and must not fabricate a reset.
 const bool hasCursor=v.resumed||v.completed;
#if !defined(LQP_OLD_ORACLE)
 if(hasCursor)require(v.resumed>=progress.stored,"local prefix progress reset between opportunities");
#endif
 require(v.resumed+v.completed<=std::get<semantic::SweptCircleMask>(geometry).segments.size(),"prefix observer exceeds actual geometry");
 if(hasCursor)progress.stored=v.resumed+v.completed;
 progress.observations.push_back({{"resumed",v.resumed},{"completed",v.completed},{"stored",progress.stored},{"steps",v.steps}});
 progress.completed+=v.completed;++progress.calls;
}
SkPath originalPath(const semantic::EraseMaskGeometry& mask){
 SkPath accumulated;
 const auto& sweep=std::get<semantic::SweptCircleMask>(mask);
 for(const auto&s:sweep.segments){SkPathBuilder b;b.moveTo(float(s.p0.position.x),float(s.p0.position.y));b.cubicTo(float(s.control1.x),float(s.control1.y),float(s.control2.x),float(s.control2.y),float(s.p1.position.x),float(s.p1.position.y));SkPaint p;p.setStyle(SkPaint::kStroke_Style);p.setStrokeWidth(float(2*std::max(s.p0.radius,s.p1.radius)));p.setStrokeCap(SkPaint::kRound_Cap);p.setStrokeJoin(SkPaint::kRound_Join);const auto part=skpathutils::FillPathWithPaint(b.detach(),p);auto next=Op(accumulated,part,kUnion_SkPathOp);require(bool(next),"original sequential union failed");accumulated=*next;}
 return accumulated;
}
semantic::EraseMaskGeometry input(unsigned count,unsigned local){std::vector<foundation::WorldPoint>trace{{420,160}};for(unsigned i=1;i<=count;++i)trace.push_back({float(420+7*std::sin(i*.17)),float(160+170.*i/1600)});return *interaction::partialEraseGeometry(trace,{1,0,0,1,double(local)*.125,double(local)*.25},16);}
}
#if !defined(LQP_OLD_ORACLE)
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
#endif
int main(int argc,char**argv){try{
 require(argc==2,"output.json");
#if !defined(LQP_OLD_ORACLE)
 edgeCases();
#endif
 auto budget=std::make_shared<resources::ResourceBudgetManager>(resources::BudgetLimits{256U*1024U*1024U,512U*1024U*1024U});EraseMaskPathCache cache;
 longQueryPreparationObserver=observePreparation;
 std::array<PrefixProgress,16> progress{};
 // Preserve the original growing append sequence. Each real call retains the
 // production max-eight-step AND eight-millisecond ceiling.
 for(unsigned n=8;n<=1600;n+=8)for(unsigned local=0;local<16;++local)
  prepareObserved(cache,input(n,local),budget,progress[local]);
 json afterAppend=json::array();for(const auto& v:progress)afterAppend.push_back(v.stored);
 // A deadline is a ceiling, not a promise to finish all eight steps. Give all
 // locals bounded further opportunities, round-robin, before ANY final get.
 std::array<unsigned,16> extra{};unsigned rounds=0;
 for(;rounds<512;++rounds){
  bool work=false;
  for(unsigned local=0;local<16;++local)if(progress[local].stored<1600){
   work=true;++extra[local];prepareObserved(cache,input(1600,local),budget,progress[local]);
  }
  if(!work)break;
 }
 json locals=json::array();std::uint64_t totalRemainder=0;bool pathsEqual=true,allPrepared=true;
 const auto before=cache.work();
 for(unsigned local=0;local<16;++local){
  allPrepared=allPrepared&&progress[local].stored==1600;
  const auto geometry=input(1600,local);const auto oldWork=cache.work();auto entry=cache.get(geometry);
  require(bool(entry),"final query path");const auto remainder=cache.work().history.queryUnionOps-oldWork.history.queryUnionOps;
  const bool equal=longQueryPreparationPath(entry)==originalPath(geometry);pathsEqual=pathsEqual&&equal;totalRemainder+=remainder;
  locals.push_back({{"local",local},{"observed_stored",progress[local].stored},{"completed_unions",progress[local].completed},
   {"calls",progress[local].calls},{"observations",progress[local].observations},{"extra_opportunities",extra[local]},{"get_union_delta",remainder},{"path_equal",equal}});
 }
 longQueryPreparationObserver=nullptr;
 json result{{"status",totalRemainder||!allPrepared||!pathsEqual?"RED":"GREEN"},{"prepared_segments",before.history.queryUnionOps},
  {"final_remainder_unions",totalRemainder},{"path_equal",pathsEqual},{"all16_prepared",allPrepared},{"after_append_stored",afterAppend},
  {"extra_rounds",rounds},{"max_extra_opportunities_per_local",512},{"locals",locals},{"resident",cache.work().bytes}};
 cache.clear();require(budget->used()==0,"prefix lease leak");
#if !defined(LQP_OLD_ORACLE)
 // Deliberately incomplete preparation: final get must perform exactly the
 // true remaining original unions, never call a partial prefix a complete hit.
 EraseMaskPathCache partial;const auto partialGeometry=input(1600,0);
 ReferenceTraversalEntry entry;entry.presentationEraseActive=true;entry.presentationEraseMasks={{{},partialGeometry}};
 RenderPreparationBudget one;one.maxCoverageSteps=1;
 std::size_t partialStored=0;unsigned partialOpportunities=0;
 longQueryPreparationObserver=observePreparation;
 for(;partialOpportunities<32&&partialStored==0;++partialOpportunities){
  PreparationAllowance allowance(one);observedPreparation=false;
  partial.prepareQueryPrefixes({&entry,1},budget,8*1024*1024,allowance);
  // A fresh opportunity may legally expire before its first union as well.
  require(allowance.steps<=1,"partial shared one-step allowance exceeded");
  if(observedPreparation)partialStored=lastObservation.resumed+lastObservation.completed;
 }
 longQueryPreparationObserver=nullptr;require(partialStored==1,"bounded partial opportunities did not retain exactly one segment");
 const auto partialBefore=partial.work();auto final=partial.get(partialGeometry);require(bool(final),"partial final path");
 const auto partialDelta=partial.work().history.queryUnionOps-partialBefore.history.queryUnionOps;
 const bool partialEqual=longQueryPreparationPath(final)==originalPath(partialGeometry);
 result["intentional_partial"]={{"opportunities",partialOpportunities},{"prepared",partialStored},{"get_union_delta",partialDelta},{"expected_remainder",1600-partialStored},{"path_equal",partialEqual}};
 final.reset();partial.clear();require(budget->used()==0,"partial prefix lease leak");
 require(partialDelta==1600-partialStored&&partialEqual,"partial final get did not compute exact true remainder");
#endif
 std::ofstream(argv[1])<<result.dump(2)<<'\n';std::cout<<result.dump()<<'\n';
 require(pathsEqual,"prefix path changed original f32 sequential PathOps");
 require(totalRemainder==0,"MULTI_LOCAL_PREFIX_LOST_BEFORE_FINAL_UP");
 require(allPrepared,"BOUNDED_PREPARATION_DID_NOT_COMPLETE_ALL16");return 0;
}catch(const std::exception&e){longQueryPreparationObserver=nullptr;std::cerr<<e.what()<<'\n';return 1;}}
