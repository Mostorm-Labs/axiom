#define main priorCpuFixture
#include "ur2d_selection_up_test.cpp"
#undef main
#include "../src/cpu_worker_protocol.hpp"
#include "../../interaction/src/partial_erase_geometry.hpp"
#include "../src/cpu_history_observer.hpp"
namespace {unsigned long long builds=0;void kernel(CpuHistoryKernelObservation o){builds+=o.unions;}}
int main(){try{
 auto budget=std::make_shared<resources::ResourceBudgetManager>(resources::BudgetLimits{256U*1024U*1024U,512U*1024U*1024U});
 EraseMaskPathCache cache;cache.setResidentUpperBound(64U*1024U*1024U);
 std::vector<foundation::WorldPoint> trace;for(unsigned i=0;i<=7200;++i)trace.push_back({float(420+7*std::sin(i*.17)),float(160+170.*i/7200)});
 std::vector<std::shared_ptr<const EraseMaskPathCache::Entry>> pins;std::vector<semantic::EraseMaskGeometry> values;
 for(unsigned local=0;local<16;++local){auto g=*interaction::partialEraseGeometry(trace,{1,0,0,1,double(local)*.125,double(local)*.25},16);CpuExactPathJob job(g);while(!job.ready())require(job.advance(8,2000),"boundedworker");auto pin=cache.publishFinished(g,job.path(),budget,64U*1024U*1024U);require(bool(pin),"16geometryfullresidency");pins.push_back(pin);values.push_back(std::move(g));}
 require(cache.work().bytes<=64U*1024U*1024U&&budget->used()<=512U*1024U*1024U,"budget");const auto retained=cache.work().bytes;
 for(unsigned i=0;i<16;++i){auto pin=cache.lookup(values[i]);require(pin==pins[i],"fullgeometrylookup");auto fork=values[i];std::get<semantic::SweptCircleMask>(fork).segments.back().control2.x+=.001;require(!cache.lookup(fork),"geometryforkfalsehit");}
 cache.clear();require(budget->used()==retained,"clearreleasedheldpins");for(unsigned i=0;i<16;++i)require(cache.lookup(values[i])==pins[i],"heldpinlostdiscoverability");
 pins.clear();require(cache.work().bytes==0&&budget->used()==0,"lastpinrelease");
 // Repeatedclear withlivepins still counts everyuniqueEntry against512.
 semantic::SweptCircleMask tiny;tiny.segments.push_back({{{1,1},1},{{2,2},1},{1.3,1.3},{1.6,1.6}});
 CpuExactPathJob tinyJob(tiny);while(!tinyJob.ready())require(tinyJob.advance(8,2000),"tinyjob");
 std::vector<std::shared_ptr<const EraseMaskPathCache::Entry>> held;
 for(unsigned i=0;i<512;++i){auto v=tiny;v.segments[0].control1.x+=i*.001;auto pin=cache.publishFinished(v,tinyJob.path(),budget,64U*1024U*1024U);require(bool(pin),"512heldadmission");held.push_back(pin);cache.clear();}
 auto extra=tiny;extra.segments[0].control1.y+=1;require(!cache.publishFinished(extra,tinyJob.path(),budget,64U*1024U*1024U),"clearbypassed512cardinality");held.clear();cache.clear();require(budget->used()==0,"512lastrelease");
 // Explicit8MiB is respected evenon64instance; CPUconstructionstill8policy.
 auto g=values.front();CpuExactPathJob job(g);while(!job.ready())require(job.advance(8,2000),"bounded");
 auto pin=cache.publishFinished(g,job.path(),budget,8U*1024U*1024U);require(bool(pin),"explicit8importfitsactual");auto other=values.back();builds=0;cpuHistoryKernelObserver=kernel;require(!cache.get(other,true),"64residencyexpandedCPUconstruction");cpuHistoryKernelObserver=nullptr;require(builds==0,"wholeCPUconstructiononmiss");pin.reset();cache.clear();require(budget->used()==0,"quota0");
 std::cout<<"{\"distinct7200\":16,\"resident_bytes\":"<<retained<<",\"clear_held\":true,\"fullvalue\":true,\"construct8\":true,\"lease0\":true}\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
