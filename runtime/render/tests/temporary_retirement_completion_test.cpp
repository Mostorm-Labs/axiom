#define main temporaryCompletionPriorFixtureMain
#include "canonical_prepared_body_test.cpp"
#undef main
#include "../src/transient_display_state.hpp"
#include "../src/gpu_resource_retirement.hpp"
#include <nlohmann/json.hpp>
#include <iostream>
using nlohmann::json;
#if defined(__APPLE__)
namespace {
struct ControlledCompletion final:GpuCompletionService {
 std::shared_ptr<NativeMetalCompletion> real;std::shared_ptr<GpuCompletionState> flags=std::make_shared<GpuCompletionState>();bool enabled=true;
 explicit ControlledCompletion(std::shared_ptr<NativeMetalCompletion> x):real(std::move(x)){}
 void service()noexcept override{if(enabled)real->service();}
 std::shared_ptr<const GpuCompletionState> state()const noexcept override{return flags;}
};
struct Target final:SkiaSurfaceProvider {
 NativeMetalTarget actual;std::shared_ptr<ControlledCompletion> completion;
 explicit Target(std::shared_ptr<NativeMetalCompletion> c):actual(c),completion(std::make_shared<ControlledCompletion>(std::move(c))){}
 std::shared_ptr<GpuCompletionService> gpuCompletionService()const noexcept override{return completion;}
 RenderTargetInfo describe()const noexcept override{return actual.describe();}SkiaSurfaceAcquireResult acquire()noexcept override{return actual.acquire();}void release()noexcept override{actual.release();}
 BackendSubmissionResult present()noexcept override{return actual.present();}BackendSubmissionResult resize(std::uint32_t w,std::uint32_t h)noexcept override{return actual.resize(w,h);}BackendSubmissionResult readbackRgba(std::span<std::uint8_t>b)noexcept override{return actual.readbackRgba(b);}std::uint64_t generation()const noexcept override{return actual.generation();}BackendSubmissionResult advanceGeneration()noexcept override{return actual.advanceGeneration();}std::uint64_t readbackCount()const noexcept override{return actual.readbackCount();}std::uint64_t cpuCopyCount()const noexcept override{return actual.cpuCopyCount();}std::uint64_t presentCount()const noexcept override{return actual.presentCount();}
};
FramePlan completionPlan(){auto e=source();auto c=std::get<CompactStrokeReferenceCommand>(e.command).content.get();c.stroke.knots.resize(256);e.record.content=c;e.command=CompactStrokeReferenceCommand{scene::ImmutableValue<semantic::CompactStrokeContent>::alias(e.record.content)};auto base=renderPlan(e,1,1);auto draw=base.referenceDrawList;for(unsigned i=1;i<4;++i){auto next=e;next.record.objectId=ObjectId::fromUint64(i+1);draw.entries.push_back(std::move(next));}return{base.frame,std::move(draw),{},false,true};}
}
#endif
int main(int argc,char**argv){try{
#if defined(__APPLE__)
 auto autorelease=message(message(reinterpret_cast<id>(objc_getClass("NSAutoreleasePool")),"alloc"),"init");auto d=MTLCreateSystemDefaultDevice(),q=message(d,"newCommandQueue");auto metal=std::make_shared<NativeMetalCompletion>();GrMtlBackendContext backend{sk_cfp<GrMTLHandle>(d),sk_cfp<GrMTLHandle>(q)};metal->context=GrDirectContexts::MakeMetal(backend);require(bool(metal->context),"actual Metal completion context");json rows=json::array();
 for(bool staged:{false,true})for(bool canonical:{false,true})for(const std::string mode:{"failed_submit","unknown_abandon","physical_loss"}){
  Target target(metal);require(target.resize(256,128).code==BackendSubmissionCode::kAccepted,"completion target");auto p=completionPlan();EditingOverlay overlay{p.frame};SkiaRenderer renderer;RenderCacheOptions options;if(staged)options.maxEntries=1;renderer.setRenderCacheOptions(options);
  // Existing native-only budget observation is installed by the transient path.
  for(unsigned n=0;;++n){const auto r=renderer.renderTransientOverlay(target,p,&overlay);renderer.serviceResourceRetirements();if(r.code==BackendSubmissionCode::kAccepted)break;require(r.code==BackendSubmissionCode::kRetry&&n<2048,"warm completion target");}
  auto budget=transientDisplayBudget.lock();require(bool(budget),"existing private budget observer");const auto presents=target.presentCount();std::vector<std::uint8_t> before(256*128*4),after(before.size());require(target.readbackRgba(before).code==BackendSubmissionCode::kAccepted,"prior pixels");
  auto old=std::make_shared<ControlledCompletion>(metal);old->enabled=false;
  std::shared_ptr<GpuResourceRetirement::Ticket> ticket;
  temporaryRetirementFault=[&](auto& queue,const auto& actualBudget){
   require(actualBudget==budget,"fault changed existing owner");ticket=queue.begin(budget,old);require(bool(ticket),"actual old ticket admission");ticket->expectSubmission();
   const auto bytes=96U*1024U*1024U-budget->used()-65536;auto token=budget->reserve(resources::ResourceClass::kNavigation,bytes,true);require(bool(token),"real released old quota");resources::ResourceLease pressure(budget,*token);ticket->end();pressure.reset();
   if(mode=="failed_submit")ticket->submitted(false);else{ticket->submitted(true);old->flags->abandoned=true;if(mode=="physical_loss")old->flags->physicalLost=true;}
   ticket->finish();
  };
  RenderPreparationBudget slice;auto r=canonical?renderer.renderFrame(target,p,nullptr,&slice):renderer.renderTransientOverlay(target,p,&overlay);temporaryRetirementFault={};
  require(!target.completion->flags->abandoned.load(),"current provider must remain healthy");
  if(mode=="physical_loss"){
   for(unsigned n=0;r.code==BackendSubmissionCode::kRetry&&n<2048;++n)r=canonical?renderer.renderFrame(target,p,nullptr,&slice):renderer.renderTransientOverlay(target,p,&overlay);
   require(r.code==BackendSubmissionCode::kAccepted&&budget->gpuEpochRetired(ticket->epoch),"physical lost old epoch did not recover healthy owner");
  }else require(r.code==BackendSubmissionCode::kRejected&&target.presentCount()==presents&&!budget->gpuEpochRetired(ticket->epoch),"blocked old queue did not terminate healthy-provider owner retry");
  require(target.readbackRgba(after).code==BackendSubmissionCode::kAccepted&&after==before,"completion outcome changed qualified scene pixels");
  rows.push_back({{"mode",mode},{"staged",staged},{"canonical",canonical},{"status",unsigned(r.code)},{"message",r.message},{"current_provider_healthy",true},{"old_epoch_retired",budget->gpuEpochRetired(ticket->epoch)},{"present_delta",target.presentCount()-presents},{"pixels_unchanged",true}});budget->close();renderer.serviceResourceRetirements();ticket.reset();

 }
 if(argc>1)std::ofstream(argv[1])<<rows.dump(2)<<'\n';std::cout<<rows.dump()<<'\n';metal.reset();message(autorelease,"drain");return 0;
#else
 return 2;
#endif
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
