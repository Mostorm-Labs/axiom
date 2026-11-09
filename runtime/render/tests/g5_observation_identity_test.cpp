#include "canvas/foundation/internal/g5_observation.hpp"
#include "canvas/semantic/document_id.hpp"
#include "canvas/semantic/operation_id.hpp"
#include <nlohmann/json.hpp>
#include <cstdlib>
#include <iostream>
#include "../src/gpu_resource_retirement.hpp"
namespace {
class ProbeService final:public canvas::render::GpuCompletionService {
 public:
  std::shared_ptr<canvas::render::GpuCompletionState> status=std::make_shared<canvas::render::GpuCompletionState>();
  void service()noexcept override{}
  std::shared_ptr<const canvas::render::GpuCompletionState> state()const noexcept override{return status;}
};
int quotaProbe(std::size_t cap,bool observe){
#if defined(AXIOM_G5_UR0_DIAGNOSTICS)
 if(canvas::foundation::internal::g5::control(2,55)||canvas::foundation::internal::g5::control(observe?1:0,0))return 2;
#else
 (void)observe;
#endif
 std::cout<<"[";
 for(unsigned mode=0;mode<3;++mode){
  using namespace canvas::resources;
  auto budget=std::make_shared<ResourceBudgetManager>(BudgetLimits{cap,cap});
  auto service=std::make_shared<ProbeService>();canvas::render::internal::GpuResourceRetirement queue;
  auto ticket=queue.begin(budget,service);
  const auto charged=budget->used();
  std::size_t delayed=charged,completed=charged,closed=charged;
  if(ticket){
#if defined(AXIOM_G5_UR0_DIAGNOSTICS)
   if(observe)canvas::foundation::internal::g5::gpuTicketPrepared(reinterpret_cast<std::uintptr_t>(ticket.get()),17,ticket->epoch);
#endif
   ticket->end();ticket->submitted(true);queue.service();delayed=budget->used();
   service->status->abandoned=mode!=0;service->status->physicalLost=mode==2;
   ticket->finish();queue.service();completed=budget->used();budget->close();queue.service();closed=budget->used();
  }
  std::cout<<(mode?",":"")<<"{\"admitted\":"<<(ticket?"true":"false")<<",\"charged\":"<<charged<<",\"delayed\":"<<delayed<<",\"completed\":"<<completed<<",\"closed\":"<<closed<<"}";
 }
 std::cout<<"]\n";return 0;
}
}
#if defined(AXIOM_G5_UR0_DIAGNOSTICS)
using namespace canvas::foundation::internal::g5;
void need(bool yes){if(!yes){std::cerr<<"FAIL: scoped identity changed\n";std::exit(1);}}
int main(int argc,char** argv){
 if(argc>1&&std::string(argv[1])=="--quota")return quotaProbe(argc>2?std::strtoull(argv[2],nullptr,10):1024,argc>3&&std::string(argv[3])=="ON");
 canvas::foundation::ObjectId raw{};raw.bytes[0]=0xab;raw.bytes[15]=0xef;
 need(word(canvas::semantic::DocumentId{raw},0)==171);
 need(word(canvas::semantic::OperationId{raw},1)==17221764975064776704ULL);
 need(control(2,1)==0&&control(1,0)==0);
 std::uint64_t parent=0;
 {
  Scope outer{Tag::canonical,Purpose::canonical};RouteScope route{Route::canonical_prepared};parent=context().job;
  need(parent!=0);need(control(3,0)!=0);
  {Scope inner{Tag::exact_prepare,Purpose::exact_query};need(context().job!=parent);emit(Kind::Basis,Tag::canonical_basis,{18446744073709551615ULL,9},true);}
  need(context().job==parent&&context().route==Route::canonical_prepared);
  observeTarget(10,2,256,256,4,true);
 }
 need(context().job==0);need(control(4,0)==0);
 auto data=nlohmann::json::parse(bytes(),bytes()+size());unsigned starts=0,ends=0,leaves=0;
 for(const auto& r:data["records"]){starts+=r["kind"]==static_cast<unsigned>(Kind::Begin);ends+=r["kind"]==static_cast<unsigned>(Kind::End);if(r["kind"]==static_cast<unsigned>(Kind::Leaf)){++leaves;need(r["job"]==std::to_string(parent));need(r["values"][3]=="2");}}
 need(starts==2&&ends==2&&leaves==1);std::cout<<"strong Id128/scoped job/leaf/early-reentrancy PASS\n";
 need(control(0,0)==0&&control(2,2)==0&&control(1,0)==0);
 {Scope scope{Tag::canonical,Purpose::canonical};canvas::resources::ResourceBudgetManager budget{{8,16}};
  need(!budget.reserve(canvas::resources::ResourceClass::kRaster,32,true));need(budget.used()==0);}
 need(control(0,0)==0&&control(4,0)==0);data=nlohmann::json::parse(bytes(),bytes()+size());bool denied=false;
 for(const auto& r:data["records"])if(r["kind"]==static_cast<unsigned>(Kind::Counter)&&r["tag"]==static_cast<unsigned>(Tag::resource_lease)&&r["values"][0]=="7"){
  denied=true;need(r["values"][3]=="32"&&r["values"][5]=="0"&&r["values"][6]=="16");}
 need(denied);std::cout<<"actual quota-denial observation preserves rejected admission PASS\n";
}
#else
int main(int argc,char** argv){return quotaProbe(argc>2?std::strtoull(argv[2],nullptr,10):1024,false);}
#endif
