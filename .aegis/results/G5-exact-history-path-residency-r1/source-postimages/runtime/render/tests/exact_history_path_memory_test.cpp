#define main hpMemoryExistingFixtureMain
#include "ur2d_selection_up_test.cpp"
#undef main
#include "../src/exact_history_path_observer.hpp"
#include <array>
namespace {
std::shared_ptr<resources::ResourceBudgetManager> observedBudget;
std::size_t residentPeak=0,constructionPeak=0,globalPeak=0,stagingPeak=0,constructions=0;
void budgetObserve(const HistoryPathObservation& event){
 if(event.event==HistoryPathEvent::kTransfer){globalPeak=std::max(globalPeak,observedBudget->used());require(observedBudget->snapshot(resources::ResourceClass::kRecorded).staging==event.estimate,"construction quota released before resident transfer");}
 residentPeak=std::max(residentPeak,event.local);
 if(event.event==HistoryPathEvent::kConstruction){++constructions;constructionPeak=std::max(constructionPeak,event.estimate);globalPeak=std::max(globalPeak,observedBudget->used());stagingPeak=std::max(stagingPeak,observedBudget->snapshot(resources::ResourceClass::kRecorded).staging);require(stagingPeak>=event.estimate,"full construction not globally precharged");require(observedBudget->used()>=event.estimate+event.local,"resident and staging not charged concurrently");}
}
semantic::SweptCircleMask sweep(unsigned count,double offset=0){semantic::SweptCircleMask value;
 for(unsigned i=0;i<count;++i){const double x=40+offset+7*std::sin(i*.17),y=30.+i*.1;value.segments.push_back({{{x,y},5},{{x+.2,y+.1},5},{x+.07,y+.03},{x+.14,y+.07}});}return value;}
}
int main(int argc,char**argv){try{
 const auto initialize=[](EraseMaskPathCache& cache,const auto& budget,std::size_t capacity){auto record=source(1,0);semantic::FilledPathMask mask;mask.path.commands={semantic::MoveTo{{0,0}},semantic::LineTo{{1,0}},semantic::LineTo{{1,1}},semantic::ClosePath{}};(void)cache.query(record,mask,budget,capacity);cache.clear();};
 {
  auto budget=std::make_shared<resources::ResourceBudgetManager>();EraseMaskPathCache cache;initialize(cache,budget,8U*1024U*1024U);observedBudget=budget;historyPathObserver=budgetObserve;
  const auto geometry=sweep(1600);auto entry=cache.get(geometry,true);require(bool(entry),"charged long history path");require(constructions==1,"single synchronous construction");require(budget->snapshot(resources::ResourceClass::kRecorded).staging==0,"completed resident remains staging");require(residentPeak<=8U*1024U*1024U&&constructionPeak<=8U*1024U*1024U&&globalPeak<=96U*1024U*1024U,"frozen memory bounds");
  const auto oldBytes=cache.work().bytes,global=budget->used();cache.clear();require(cache.work().bytes==oldBytes&&budget->used()==global,"clear prematurely released externally pinned bytes");auto fork=geometry;fork.segments.back().control2.x+=1;auto different=cache.get(fork,true);require(different&&different!=entry,"same identity geometry fork reused different path");auto original=cache.get(geometry,true);require(original&&original!=different,"full geometry values not qualified");cache.clear();different.reset();original.reset();require(budget->used()>0,"external original pin not valid after fork/clear");entry.reset();require(budget->used()==0&&cache.work().bytes==0,"last external release leaked");historyPathObserver=nullptr;observedBudget.reset();
 }
 {
  // CPU Recorded staging cannot inherit a live pixel GPU epoch or wait for it.
  auto budget=std::make_shared<resources::ResourceBudgetManager>();EraseMaskPathCache cache;initialize(cache,budget,8U*1024U*1024U);
  const auto epoch=budget->beginGpuEpoch(resources::kPixelResourceClasses,resources::kPixelResourceClasses);require(bool(epoch),"pixel epoch");auto path=cache.get(sweep(20),true);require(bool(path),"CPU path during pixel epoch");require(budget->endGpuEpoch(*epoch),"end pixel epoch");cache.clear();path.reset();require(budget->used()==0&&budget->snapshot(resources::ResourceClass::kRecorded).pendingRelease==0,"CPU construction unexpectedly waits for GPU");require(budget->completeGpuEpoch(*epoch),"epoch complete");
 }
 {
  auto budget=std::make_shared<resources::ResourceBudgetManager>(resources::BudgetLimits{512U*1024U,512U*1024U});EraseMaskPathCache cache;initialize(cache,budget,8U*1024U*1024U);
  require(!cache.get(sweep(1600),true),"global hard denial bypassed");require(budget->used()==0,"denied build leaked");
  auto tiny=cache.get(sweep(2),true);require(bool(tiny),"global denial failed to recover small work");cache.clear();tiny.reset();require(budget->used()==0,"small recovery lease leak");
 }
 {
  auto budget=std::make_shared<resources::ResourceBudgetManager>();EraseMaskPathCache cache;initialize(cache,budget,128U*1024U);std::vector<std::shared_ptr<const EraseMaskPathCache::Entry>> external;
  for(unsigned i=0;i<64;++i){auto path=cache.get(sweep(1,i*10),true);if(!path)break;external.push_back(std::move(path));require(cache.work().bytes<=128U*1024U,"external pins exceeded local resident quota");}
  cache.clear();require(cache.work().bytes<=128U*1024U,"clear forgot externally held quota");external.clear();require(cache.work().bytes==0&&budget->used()==0,"external pressure last release");
 }
 json result{{"status","PASS"},{"resident_peak",residentPeak},{"construction_peak",constructionPeak},{"combined_owner_upper_bound",residentPeak+constructionPeak},{"global_peak",globalPeak},{"staging_peak",stagingPeak},{"resident_cap",8388608},{"single_construction",true},{"staging_to_resident_transfer",true},{"external_pins_clear_fork_release0",true},{"tiny_global_denial_recovery",true},{"CPU_not_GPU_epoch",true}};std::cout<<result.dump()<<'\n';if(argc==2)std::ofstream(argv[1])<<result.dump(2)<<'\n';return 0;
}catch(const std::exception&e){historyPathObserver=nullptr;std::cerr<<e.what()<<'\n';return 1;}}
