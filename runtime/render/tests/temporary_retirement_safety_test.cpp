#define main temporarySafetyPriorFixtureMain
#include "canonical_prepared_body_test.cpp"
#undef main
#include "../src/gpu_resource_retirement.hpp"
#include <nlohmann/json.hpp>
#include <iostream>
using nlohmann::json;
namespace canvas::render::internal {
struct TemporaryRetirementWaitProbe {
 static bool acquire(SkiaRenderCache& c,SkCanvas& canvas,sk_sp<SkSurface>& surface,resources::ResourceLease& lease,bool mandatory=true){return c.acquireWorkSurface(canvas,surface,lease,mandatory);}
 static auto failure(const SkiaRenderCache& c){return c.allocationFailure_;}
 static auto state(const SkiaRenderCache& c){return std::tuple(c.preparationCursor_,c.admissions_.size(),c.bodyDependencies_.size(),c.stagedBodies_);}
};
}
namespace {
#if defined(__APPLE__)
FramePlan safetyPlan(bool forcedStaged=false){auto e=source();auto c=std::get<CompactStrokeReferenceCommand>(e.command).content.get();c.stroke.knots.resize(64);e.record.content=c;e.command=CompactStrokeReferenceCommand{scene::ImmutableValue<semantic::CompactStrokeContent>::alias(e.record.content)};auto p=renderPlan(e,1,1);auto draw=p.referenceDrawList;for(unsigned i=1;i<4;++i){auto next=e;next.record.objectId=ObjectId::fromUint64(i+1);draw.entries.push_back(next);}return {p.frame,std::move(draw),{},false,true};}
struct Completion final:GpuCompletionService{std::shared_ptr<GpuCompletionState> flags=std::make_shared<GpuCompletionState>();void service()noexcept override{}std::shared_ptr<const GpuCompletionState> state()const noexcept override{return flags;}};
json cachePressure(std::shared_ptr<NativeMetalCompletion> metal,const std::string& mode){
 NativeMetalTarget target(metal);require(target.resize(256,128).code==BackendSubmissionCode::kAccepted,"safety target");auto surface=target.acquire().frame.surface;auto plan=safetyPlan();SkiaRenderCache c;c.budget=std::make_shared<resources::ResourceBudgetManager>();c.preparedGeometry=std::make_shared<PreparedGeometryCache>();c.sourcePool=std::make_shared<SourceRetentionPool>();c.preparedGeometry->setRetentionPool(c.sourcePool);c.begin(plan,&target,*surface,0);
 for(unsigned i=0;;++i){PreparationAllowance a;auto s=c.prepareBodies(*surface->getCanvas(),plan,a);if(s==SkiaRenderCache::Preparation::kReady)break;require(s==SkiaRenderCache::Preparation::kPending&&i<1024,"safety bodies");}
 const auto before=TemporaryRetirementWaitProbe::state(c);const std::size_t plane=256*128*4,hard=96U*1024U*1024U;
 std::optional<std::uint64_t> epoch;if(mode=="reclaim_pending")epoch=c.budget->beginGpuEpoch(resources::resourceClassMask(resources::ResourceClass::kNavigation),0);
 const auto token=c.budget->reserve(resources::ResourceClass::kNavigation,hard-c.budget->used()-plane/2,true);require(bool(token),"safety live pressure");resources::ResourceLease held(c.budget,*token);if(epoch)require(c.budget->endGpuEpoch(*epoch),"safety endepoch");
 unsigned reclaimCalls=0;if(epoch)c.reclaimPreparation=[&](std::size_t n){require(n==plane,"reclaim request changed");++reclaimCalls;held.reset();};
 sk_sp<SkSurface> work;resources::ResourceLease lease;const bool ok=TemporaryRetirementWaitProbe::acquire(c,*surface->getCanvas(),work,lease);require(!ok&&!work,"pressure overcommit");
 if(epoch){require(reclaimCalls==1&&c.budget->snapshot(resources::ResourceClass::kNavigation).pendingRelease>0&&!c.stagedBodies()&&TemporaryRetirementWaitProbe::state(c)==before,"reclaim-created pending destroyed dependencies");require(c.budget->completeGpuEpoch(*epoch),"reclaim true completion");}
 else {require(c.budget->pendingGpuEpochCount()==0&&c.budget->snapshot(resources::ResourceClass::kNavigation).pendingRelease==0&&c.stagedBodies(),"resident shortage did not retain staged fallback");held.reset();}
 require(TemporaryRetirementWaitProbe::acquire(c,*surface->getCanvas(),work,lease)&&work,"release recovery real allocation");work.reset();lease.reset();
 for(unsigned i=0;;++i){RenderPreparationBudget slice;PreparationAllowance a(slice);auto s=c.prepare(*surface->getCanvas(),plan,slice,nullptr,nullptr,&a);if(s==SkiaRenderCache::Preparation::kReady)break;require(s!=SkiaRenderCache::Preparation::kRejected&&i<8192,"resident staging/reclaim did not converge after pressure release");}
 c.clear();c.preparedGeometry->clear();require(c.budget->used()==0,"safety last lease leak");target.release();return {{"mode",mode},{"actual_denial",true},{"reclaim_calls",reclaimCalls},{"after_release_converged",true}};
}
json streamedTransientWait(std::shared_ptr<NativeMetalCompletion> metal,bool staged){
 NativeMetalTarget target(metal);require(target.resize(256,128).code==BackendSubmissionCode::kAccepted,"streamed transient target");auto* surface=target.acquire().frame.surface;auto plan=safetyPlan();
 SkiaRenderCache c;c.budget=std::make_shared<resources::ResourceBudgetManager>();c.preparedGeometry=std::make_shared<PreparedGeometryCache>();c.sourcePool=std::make_shared<SourceRetentionPool>();c.preparedGeometry->setRetentionPool(c.sourcePool);c.begin(plan,&target,*surface,0);
 for(unsigned i=0;;++i){PreparationAllowance a;auto status=c.prepareBodies(*surface->getCanvas(),plan,a);if(status==SkiaRenderCache::Preparation::kReady)break;require(status==SkiaRenderCache::Preparation::kPending&&i<1024,"streamed body progress");}
 if(staged)c.options.maxEntries=1;else c.options.maxBytes=1024;
 const auto epoch=c.budget->beginGpuEpoch(resources::resourceClassMask(resources::ResourceClass::kNavigation),0);require(bool(epoch),"stream epoch");
 const auto token=c.budget->reserve(resources::ResourceClass::kNavigation,96U*1024U*1024U-c.budget->used()-256*128*2,true);require(bool(token),"stream pressure");resources::ResourceLease held(c.budget,*token);require(c.budget->endGpuEpoch(*epoch),"stream epoch end");held.reset();
 for(unsigned i=0;i<3;++i){PreparationAllowance a;auto r=staged?c.prepare(*surface->getCanvas(),plan,a.limits,nullptr,nullptr,&a):c.prepareTransientMaterial(*surface->getCanvas(),plan,a,nullptr,nullptr);require(r==SkiaRenderCache::Preparation::kResourcePending&&c.stagedBodies()==staged,"streamed temporary pending converted to permanent staging");}
 require(c.budget->completeGpuEpoch(*epoch),"stream complete");
 for(unsigned i=0;;++i){PreparationAllowance a;auto r=c.prepare(*surface->getCanvas(),plan,a.limits,nullptr,nullptr,&a);if(r==SkiaRenderCache::Preparation::kReady)break;require(r!=SkiaRenderCache::Preparation::kRejected&&i<4096,"stream wait failed recovery");}
 c.clear();c.preparedGeometry->clear();require(c.budget->used()==0,"stream last lease");target.release();return {{"staged",staged},{"resource_pending_preserved",true},{"resume_after_actual_completion",true}};
}
json completionBetweenDenialAndQuery(std::shared_ptr<NativeMetalCompletion> metal){
 NativeMetalTarget target(metal);require(target.resize(256,128).code==BackendSubmissionCode::kAccepted,"race target");auto* surface=target.acquire().frame.surface;auto plan=safetyPlan();SkiaRenderCache c;c.budget=std::make_shared<resources::ResourceBudgetManager>();c.preparedGeometry=std::make_shared<PreparedGeometryCache>();c.begin(plan,&target,*surface,0);
 for(unsigned n=0;;++n){PreparationAllowance a;auto r=c.prepareBodies(*surface->getCanvas(),plan,a);if(r==SkiaRenderCache::Preparation::kReady)break;require(r==SkiaRenderCache::Preparation::kPending&&n<1024,"race body");}
 RenderPreparationBudget prefix;prefix.maxGroups=1;
 for(unsigned n=0;!c.hasPixels(plan.referenceDrawList.entries[1]);++n){PreparationAllowance a(prefix);auto r=c.prepare(*surface->getCanvas(),plan,prefix,nullptr,nullptr,&a);require(r!=SkiaRenderCache::Preparation::kRejected&&n<4096,"race completed material prefix");}
 auto before=TemporaryRetirementWaitProbe::state(c);auto epoch=c.budget->beginGpuEpoch(resources::resourceClassMask(resources::ResourceClass::kNavigation),0);require(bool(epoch),"race epoch");auto t=c.budget->reserve(resources::ResourceClass::kNavigation,96U*1024U*1024U-c.budget->used()-65536,true);require(t&&c.budget->endGpuEpoch(*epoch),"race held quota");c.budget->release(*t);
 unsigned denied=0;temporaryRetirementReserveDenied=[&]{++denied;require(c.budget->completeGpuEpoch(*epoch),"completion between denial and query");};sk_sp<SkSurface> output;resources::ResourceLease lease;
 bool ok=TemporaryRetirementWaitProbe::acquire(c,*surface->getCanvas(),output,lease);temporaryRetirementReserveDenied={};
 require(ok&&denied==1&&TemporaryRetirementWaitProbe::state(c)==before&&!c.stagedBodies()&&c.hasPixels(plan.referenceDrawList.entries[1]),"COMPLETION_BETWEEN_DENIAL_AND_QUERY_DESTROYED_PREPARATION");output.reset();lease.reset();c.clear();c.preparedGeometry->clear();require(c.budget->used()==0,"race cleanup");return {{"actual_first_reserve_failed",true},{"completion_before_query",true},{"bounded_actual_retry_success",true},{"state_preserved",true}};
}
json querySafety(){
 using resources::ResourceClass;using resources::ResourceBudgetManager;auto b=std::make_shared<ResourceBudgetManager>(resources::BudgetLimits{4096,8192});
 auto first=b->beginGpuEpoch(resources::resourceClassMask(ResourceClass::kScratch),0);auto one=b->reserve(ResourceClass::kScratch,4096,true);require(first&&one&&b->endGpuEpoch(*first),"first real epoch");
 auto second=b->beginGpuEpoch(resources::resourceClassMask(ResourceClass::kNavigation),0);auto two=b->reserve(ResourceClass::kNavigation,4096,true);require(second&&two&&b->endGpuEpoch(*second),"second real epoch");
 require(!b->reserve(ResourceClass::kScratch,4096,true)&&!(b->reserveAvailability(ResourceClass::kScratch,4096,true)==ResourceBudgetManager::ReserveAvailability::kAfterPendingRelease),"live epochs mistaken for pending bytes");
 b->release(*one);b->release(*two);require((b->reserveAvailability(ResourceClass::kScratch,4096,true)==ResourceBudgetManager::ReserveAvailability::kAfterPendingRelease),"released quota unavailable");
 require(b->completeGpuEpoch(*second)&&b->used()==8192&&!b->reserve(ResourceClass::kScratch,4096,true),"later completion released unordered prefix");
 require(b->completeGpuEpoch(*first)&&b->used()==0,"ordered completion did not release");auto ready=b->reserve(ResourceClass::kScratch,4096,true);require(bool(ready),"actual allocation after completion");b->release(*ready);
 require(!(b->reserveAvailability(ResourceClass::kScratch,8193,true)==ResourceBudgetManager::ReserveAvailability::kAfterPendingRelease)&&!(b->reserveAvailability(ResourceClass::kCount,1,true)==ResourceBudgetManager::ReserveAvailability::kAfterPendingRelease),"invalid request admitted for wait");
 auto closed=std::make_shared<ResourceBudgetManager>(resources::BudgetLimits{4096,8192});auto e=closed->beginGpuEpoch();auto t=closed->reserve(ResourceClass::kScratch,8192,true);require(e&&t&&closed->endGpuEpoch(*e),"closed setup");closed->release(*t);closed->close();require(!closed->reserve(ResourceClass::kScratch,1,true)&&!(closed->reserveAvailability(ResourceClass::kScratch,1,true)==ResourceBudgetManager::ReserveAvailability::kAfterPendingRelease),"closed budget hid failure as wait");
 return {{"live_epochs_not_pending_bytes",true},{"out_of_order_no_early_quota",true},{"actual_reserve_after_ordered_completion",true},{"closed_invalid_oversize_not_wait",true}};
}
json completionSafety(){json rows=json::array();for(std::string mode:{"failed_submit","unknown_abandon","physical_loss"}){
 auto b=std::make_shared<resources::ResourceBudgetManager>(resources::BudgetLimits{4096,8192});GpuResourceRetirement queue;auto service=std::make_shared<Completion>();auto ticket=queue.begin(b,service);require(bool(ticket),"ticket safety setup");ticket->expectSubmission();auto id=b->reserve(resources::ResourceClass::kScratch,4096,true);require(bool(id),"ticket lease");resources::ResourceLease lease(b,*id);ticket->end();lease.reset();
 if(mode=="failed_submit")ticket->submitted(false);else{ticket->submitted(true);service->flags->abandoned=true;if(mode=="physical_loss")service->flags->physicalLost=true;}
 ticket->finish();queue.service();
 if(mode=="physical_loss")require(b->gpuEpochRetired(ticket->epoch)&&!queue.blocked(),"physical loss did not recover quota");
 else require(!b->gpuEpochRetired(ticket->epoch)&&queue.blocked()&&!queue.pending()&&b->snapshot(resources::ResourceClass::kScratch).pendingRelease==4096,"terminal completion mistaken for qualified wait");
 rows.push_back({{"mode",mode},{"blocked",queue.blocked()},{"needs_service",queue.pending()},{"retired",b->gpuEpochRetired(ticket->epoch)}});b->close();queue.service();ticket.reset();require(b->used()==0,"terminal cleanup leaked");
 }return rows;}
#endif
}
int main(int argc,char**argv){try{
#if defined(__APPLE__)
 auto autorelease=message(message(reinterpret_cast<id>(objc_getClass("NSAutoreleasePool")),"alloc"),"init");auto d=MTLCreateSystemDefaultDevice(),q=message(d,"newCommandQueue");auto metal=std::make_shared<NativeMetalCompletion>();GrMtlBackendContext backend{sk_cfp<GrMTLHandle>(d),sk_cfp<GrMTLHandle>(q)};metal->context=GrDirectContexts::MakeMetal(backend);require(bool(metal->context),"REQUIRED_ACTUAL_METAL_CONTEXT_UNAVAILABLE");
 json report={{"actual_Metal",true},{"resident",cachePressure(metal,"resident")},{"after_reclaim",cachePressure(metal,"reclaim_pending")},{"transient_stream",streamedTransientWait(metal,false)},{"canonical_staged",streamedTransientWait(metal,true)},{"completion_race",completionBetweenDenialAndQuery(metal)},{"query",querySafety()},{"completion",completionSafety()}};if(argc>1)std::ofstream(argv[1])<<report.dump(2)<<'\n';std::cout<<report.dump()<<'\n';metal.reset();message(autorelease,"drain");return 0;
#else
 return 2;
#endif
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
