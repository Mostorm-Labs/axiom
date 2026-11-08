#include <cstdlib>
#include <iostream>
#if !__has_include("canvas/foundation/internal/g5_observation.hpp")
int main(){std::cerr<<"FAIL: private observation collector is unavailable\n";return 1;}
#else
#include "canvas/foundation/internal/g5_observation.hpp"
#include <nlohmann/json.hpp>
#include <thread>
#include "canvas/resources/resource_budget_manager.hpp"
using namespace canvas::foundation::internal::g5;
using Json=nlohmann::json;
void check(bool yes,const char* reason){if(!yes){std::cerr<<"FAIL: "<<reason<<'\n';std::exit(1);}}
Json page(){check(control(4,0)==0,"drain rejected");return Json::parse(bytes(),bytes()+size());}
int main(){
 check(control(2,17)==0,"reset");
 check(control(0,0)==0,"off");
 unsigned evaluated=0;G5_OBSERVE(Kind::Stage,Tag::input,false,++evaluated);check(evaluated==0,"OFF evaluated arguments");
 emit(Kind::Stage,3,{42},false);auto off=page();check(off["records"].empty(),"OFF recorded work");
 check(control(2,18)==0&&control(1,0)==0,"enable");
 check(control(5,57)==0,"begin event");
 emit(Kind::Stage,7,{18446744073709551615ULL,9},false);
 emit(Kind::Terminal,8,{4},true);
 auto p=page();check(p["records"].size()==2,"lost records");
 check(p["records"][0]["event"]=="57","event identity");
 check(p["records"][0]["values"][0]=="18446744073709551615","u64 lost precision");
 auto copied=p.dump();check(control(3,0)==0,"clock");check(Json::parse(bytes(),bytes()+size()).contains("clock_ns"),"clock data");check(!copied.empty(),"caller copy");
 check(control(2,19)==0,"reset after drain");check(control(1,0)==0,"on");
 for(unsigned i=0;i<200000;++i)emit(Kind::Stage,1,{i},false);
 emit(Kind::Terminal,2,{123},true);
 bool terminal=false;std::uint64_t lost=0;
 for(unsigned i=0;i<2048;++i){auto d=page();lost=d["loss"]["statistical"].get<std::uint64_t>();
   for(const auto& r:d["records"])if(r["kind"]==static_cast<unsigned>(Kind::Terminal))terminal=true;
   if(d["finished"].get<bool>())break;
 }
 check(lost>0,"overflow not reported");check(terminal,"ordinary overflow overwrote terminal");
 check(control(2,20)==0&&control(1,0)==0,"gpu run");
 auto token=gpuSubmitted(10,4);check(control(2,21)!=0,"reset invalidated live GPU state");
 std::thread complete([&]{gpuCompleted(token,false,false);});complete.join();
 auto g=page();check(g["pending_gpu"]==0,"GPU completion not observed");
 check(control(2,21)!=0,"reset exceeded aggregate cap with a pinned completed run");
 gpuRetired(token,4);
 check(control(2,22)==0&&control(1,0)==0,"prepared GPU run");
 // Registration is not submission. A synchronous finished callback may run
 // inside flush; retain both actual observations without inventing their order.
#if defined(AXIOM_G5_PREPARED_GPU_OBSERVATION)
 auto prepared=gpuPrepared(11,5);auto onlyPrepared=page();
 for(const auto& r:onlyPrepared["records"])check(r["kind"]!=static_cast<unsigned>(Kind::GpuSubmit),"GPU counted before actual submit");
 gpuCompleted(prepared,false,false);gpuSubmissionObserved(prepared,true);
 auto completedBeforeSubmit=page();check(completedBeforeSubmit["records"].size()==2,"actual callback/submission witnesses lost");
 check(completedBeforeSubmit["records"][0]["kind"]==static_cast<unsigned>(Kind::GpuComplete),"callback order rewritten");
 check(completedBeforeSubmit["records"][1]["kind"]==static_cast<unsigned>(Kind::GpuSubmit),"actual submit not observed");
 gpuRetired(prepared,5);
#else
 check(false,"GPU registration/submission observations not separated");
#endif
 check(control(2,23)==0&&control(1,0)==0,"lease run");
#if defined(AXIOM_G5_LEASE_OBSERVATIONS)
 {canvas::resources::ResourceBudgetManager quota({1024,1024});
  const auto id=quota.reserve(canvas::resources::ResourceClass::kRaster,10,true,6);check(id.has_value(),"lease reserve");
  const auto epoch=quota.beginGpuEpoch();check(epoch.has_value(),"lease epoch");check(quota.endGpuEpoch(*epoch),"lease end");quota.release(*id);
  auto held=page();bool freed=false,reserved=false;for(const auto& r:held["records"])if(r["tag"]==static_cast<unsigned>(Tag::resource_lease)){
   const auto& v=r["values"];freed|=v[0]=="3";reserved|=v[0]=="1"&&v[4]=="10"&&v[5]=="6";
  }
  check(reserved&&!freed,"logical release reported physical quota retirement");check(quota.completeGpuEpoch(*epoch),"lease complete");
  auto retired=page();for(const auto& r:retired["records"])if(r["tag"]==static_cast<unsigned>(Tag::resource_lease))freed|=r["values"][0]=="3";
  check(freed&&quota.used()==0,"actual lease retirement unobserved");
 }
#else
 check(false,"actual resource lease observation unavailable");
#endif
 check(control(2,24)==0&&control(1,0)==0,"coverage aggregate run");
 {Scope root{Tag::canonical,Purpose::canonical};for(unsigned i=0;i<5000;++i){Scope item{Tag::coverage};}}
 auto dense=page();check(dense["records"].size()<20&&dense["finished"].get<bool>(),"inner coverage flooded the bounded collector");
 bool aggregate=false;for(const auto& r:dense["records"])if(r["kind"]==static_cast<unsigned>(Kind::Counter)&&r["tag"]==static_cast<unsigned>(Tag::coverage))aggregate|=r["values"][0]=="9"&&r["values"][3]=="5000";
 check(aggregate,"coverage invocation total lost");
 check(control(2,25)==0&&control(1,0)==0,"target aggregation run");
 {Scope root{Tag::canonical,Purpose::canonical};for(unsigned i=0;i<5000;++i)observeTarget(123,2,256,256,4,true);}
 auto targets=page();check(targets["records"].size()<20,"identical target uses flooded critical records");
 bool counted=false;for(const auto& r:targets["records"])if(r["kind"]==static_cast<unsigned>(Kind::Counter)&&r["tag"]==static_cast<unsigned>(Tag::target))counted|=r["values"][9]=="5000";
 check(counted,"actual repeated target use count lost");
 check(control(2,26)==0&&control(1,0)==0,"critical priority run");
 for(unsigned i=0;i<200000;++i)emit(Kind::Counter,Tag::lookup,{i},false);
 for(unsigned i=0;i<15000;++i)emit(Kind::Terminal,Tag::input_result,{i},true);
 std::uint64_t terminals=0,lastSequence=0;bool firstRecord=true;
 for(unsigned i=0;i<4096;++i){auto p=page();check(p["loss"]["critical"]==0,"ordinary data prevented critical retention");
  for(const auto& r:p["records"]){const auto seq=std::stoull(r["seq"].get<std::string>());check(firstRecord||seq>lastSequence,"priority drain lost sequence order");firstRecord=false;lastSequence=seq;terminals+=r["kind"]==static_cast<unsigned>(Kind::Terminal);}
  if(p["finished"].get<bool>())break;
 }
 check(terminals==15000,"critical terminal overwritten by ordinary data");
 check(control(2,27)==0&&control(1,0)==0,"bounded GPU sidecar run");
 gpuTicketPrepared(1001,10,4);
 check(control(2,28)!=0,"reset released live sidecar");
 std::thread ticketDone([]{gpuTicketCompleted(1001,false,false);});ticketDone.join();
 gpuTicketCompleted(1001,false,false);gpuTicketSubmitted(1001,true);
 check(control(2,28)!=0,"completed sidecar unpinned before retirement");
 gpuTicketRetired(1001,4);auto sidecar=page();unsigned doneCount=0;
 for(const auto& r:sidecar["records"])doneCount+=r["kind"]==static_cast<unsigned>(Kind::GpuComplete);
 check(doneCount==1&&sidecar["pending_gpu"]==0,"sidecar lost/duplicated actual completion");
 check(control(2,28)==0&&control(1,0)==0,"reset after sidecar retirement");
 for(unsigned i=1;i<=1024;++i)gpuTicketPrepared(i,10,i);
 gpuTicketPrepared(2048,10,2048);auto fullSidecar=page();
 check(fullSidecar["loss"]["critical"]==1,"sidecar exhaustion not explicit");
 for(unsigned i=1;i<=1024;++i){gpuTicketCompleted(i,i%2==0,i%3==0);gpuTicketRetired(i,i);}
 check(control(2,29)==0&&control(1,0)==0,"dense recorded lease run");
 {Scope root{Tag::canonical,Purpose::canonical};canvas::resources::ResourceBudgetManager quota({1024,1024});
  for(unsigned i=0;i<40000;++i){const auto id=quota.reserve(canvas::resources::ResourceClass::kRecorded,10,true,6);check(id.has_value(),"model reserve");quota.release(*id);}
 }
 unsigned cycles=0;std::uint64_t previousId=0;
 for(unsigned pageIndex=0;pageIndex<4096;++pageIndex){auto d=page();check(d["loss"]["statistical"]==0&&d["loss"]["critical"]==0,"short recorded leases flooded collector");
  for(const auto& r:d["records"]){if(r["tag"]!=static_cast<unsigned>(Tag::resource_lease))continue;const auto& v=r["values"];
   check(v[0]=="6"&&v.size()==16,"short lease lifecycle not losslessly compacted");
   const auto id=std::stoull(v[2].get<std::string>());check(id==++previousId,"lease identity lost");
   check(v[4]=="10"&&v[5]=="6"&&v[7]=="10"&&v[11]=="10"&&v[12]=="10","lease bytes/peak state changed");
   check(std::stoull(v[8].get<std::string>())<=std::stoull(v[9].get<std::string>())&&std::stoull(v[9].get<std::string>())<=std::stoull(v[10].get<std::string>()),"lease event time changed");++cycles;
  }
  if(d["finished"].get<bool>())break;
 }
 check(cycles==40000,"lease lifecycle total lost");
 check(control(2,21)==0,"retired run could not reset");
 check(control(999,0)!=0,"unknown control admitted");
 std::thread foreign([]{check(control(3,0)!=0,"foreign thread control admitted");});foreign.join();
 std::cout<<"bounded collector/off/overflow/terminal/u64/buffer/GPU lifecycle PASS\n";
}
#endif
