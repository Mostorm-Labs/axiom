#define main validationPriorFixtureMain
#include "canonical_prepared_body_test.cpp"
#undef main
#include "canvas/foundation/internal/reference_validation_metrics.hpp"
#include "../src/reference_validation_cache.hpp"
#include "../src/transient_display_state.hpp"
#include "../src/cpu_worker_protocol.hpp"
#include "../src/cpu_local_exact.hpp"
#include "../src/skia_gpu_lifetime.hpp"
#include <nlohmann/json.hpp>
#include <iostream>
#include "canvas/semantic/reference_object_store.hpp"
#include <cstdlib>
namespace {thread_local bool validationFailAllocation=false;}
void* operator new(std::size_t n){if(validationFailAllocation){validationFailAllocation=false;throw std::bad_alloc();}if(auto p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void operator delete(void* p)noexcept{std::free(p);}
void operator delete(void* p,std::size_t)noexcept{std::free(p);}
namespace {
ReferenceTraversalEntry validationFixture(){
 auto e=source();auto c=std::get<CompactStrokeReferenceCommand>(e.command).content.get();c.stroke.knots.clear();c.stroke.config.size=8;c.stroke.config.opacity=.65;
 for(unsigned i=0;i<8192;++i)c.stroke.knots.push_back({8+240.*i/8191,64+24*std::sin(i*.01),32768});
 e.record.content=c;e.command=CompactStrokeReferenceCommand{scene::ImmutableValue<semantic::CompactStrokeContent>::alias(e.record.content)};
 std::vector<semantic::EraseMaskRecord> masks;
 for(unsigned j=0;j<8;++j){semantic::SweptCircleMask m;for(unsigned i=0;i<128;++i){const double x=10+220.*i/127,y=20+j*10;
  m.segments.push_back({{{x,y},1},{{x+1,y+1},1},{x+.25,y+.25},{x+.75,y+.75}});}masks.push_back({ObjectId::fromUint64(100+j),m});}
 semantic::FilledPathMask fill;fill.path.commands.push_back(semantic::MoveTo{{20,20}});for(unsigned i=1;i<128;++i)fill.path.commands.push_back(semantic::LineTo{{20.+i*.2,20.+(i%3)}});masks.push_back({ObjectId::fromUint64(200),fill});e.record.eraseMasks=std::move(masks);return e;
}
FramePlan validationPlan(unsigned copies){
 auto e=validationFixture();FrameState f{ViewId{1},CameraState{{128,64},1,0,CameraGeneration{1}},{0,0,256,128},{256,128,256,128,1,1},semantic::SemanticGeneration{1},SceneRevision{1},SurfaceGeneration{1},MetricsGeneration{1},FrameId{1},{0,0,256,128}};
 ReferenceDrawList list{f};list.viewportClip={0,0,256,128};
 for(unsigned i=0;i<copies;++i){auto a=e;a.record.objectId=ObjectId::fromUint64(i+1);list.entries.push_back(std::move(a));}
 return FramePlan{f,std::move(list),{},false,true};
}
FramePlan changedEntry(const FramePlan& p,ReferenceTraversalEntry e){auto list=p.referenceDrawList;list.entries={std::move(e)};return FramePlan{p.frame,std::move(list),p.background,p.transparentBackground,p.isolateEraseMasks};}
std::vector<std::uint8_t> validationPixels(const sk_sp<SkSurface>& s){
 auto info=SkImageInfo::Make(s->width(),s->height(),kRGBA_8888_SkColorType,kPremul_SkAlphaType);std::vector<std::uint8_t> out(info.computeMinByteSize());require(s->readPixels(info,out.data(),info.minRowBytes(),0,0),"validation pixels");return out;
}
class ValidationTarget final:public SkiaSurfaceProvider {
 public:sk_sp<SkSurface> surface;std::uint64_t epoch=1,presents=0;
 RenderTargetInfo describe()const noexcept override{RenderTargetInfo info;info.profileId="validation-test-raster";info.kind=RenderTargetKind::kCpuRaster;info.backend=RenderTargetBackend::kRaster;return info;}
 SkiaSurfaceAcquireResult acquire()noexcept override{return surface?SkiaSurfaceAcquireResult::acquired({surface.get(),epoch}):SkiaSurfaceAcquireResult{};}
 void release()noexcept override{}
 BackendSubmissionResult present()noexcept override{++presents;return BackendSubmissionResult::accepted();}
 BackendSubmissionResult resize(std::uint32_t w,std::uint32_t h)noexcept override{surface=SkSurfaces::Raster(SkImageInfo::MakeN32Premul(w,h));return BackendSubmissionResult::accepted();}
 BackendSubmissionResult readbackRgba(std::span<std::uint8_t> out)noexcept override{auto p=validationPixels(surface);std::copy(p.begin(),p.end(),out.begin());return BackendSubmissionResult::accepted();}
 std::uint64_t generation()const noexcept override{return epoch;}
 BackendSubmissionResult advanceGeneration()noexcept override{++epoch;return BackendSubmissionResult::accepted();}
 std::uint64_t readbackCount()const noexcept override{return 0;}
 std::uint64_t cpuCopyCount()const noexcept override{return 0;}
 std::uint64_t presentCount()const noexcept override{return presents;}
};
namespace metrics=foundation::internal::reference_validation_metrics;
nlohmann::json validationNegatives(){
 auto plan=validationPlan(1);ReferenceValidationCache cache;RenderCacheOptions options;
 auto budget=std::make_shared<resources::ResourceBudgetManager>();auto pool=std::make_shared<SourceRetentionPool>();cache.bind(options,budget,pool);
 const auto compare=[&](const FramePlan& p){auto a=validateReferencePlan(p);auto b=validateReferencePlan(p,&cache,HistoryValidationPolicy::kPublishedImmutable);require(a==b,"cached validator differs from original rejection/message");return a.code==BackendSubmissionCode::kAccepted;};
 require(compare(plan),"positive");metrics::counts={};require(compare(plan),"positive hit");
 // Compare calls include the independent full validator; measure cache alone
 // for isolation/invalidation work assertions.
 auto e=plan.referenceDrawList.entries.front();auto changed=e;changed.record.eraseMasks[0].id=ObjectId::fromUint64(999);
 auto masks=changedEntry(plan,changed);metrics::counts={};require(validateReferencePlan(masks,&cache,HistoryValidationPolicy::kPublishedImmutable).code==BackendSubmissionCode::kAccepted,"new history");require(metrics::counts.bodyCalls==0&&metrics::counts.maskArrays==1,"history edit revalidated body or skipped version");
 changed=e;changed.record.content.update([](auto& v){std::get<semantic::CompactStrokeContent>(v).stroke.seed++;});changed.command=CompactStrokeReferenceCommand{scene::ImmutableValue<semantic::CompactStrokeContent>::alias(changed.record.content)};
 metrics::counts={};require(validateReferencePlan(changedEntry(plan,changed),&cache,HistoryValidationPolicy::kPublishedImmutable).code==BackendSubmissionCode::kAccepted,"newbody");require(metrics::counts.bodyCalls==1&&metrics::counts.maskArrays==0,"body edit revalidated history");
 unsigned rejected=0;
 for(unsigned mode=0;mode<10;++mode){auto x=e;
  if(mode<4){auto c=std::get<CompactStrokeReferenceCommand>(x.command).content.get();if(mode==0)c.stroke.knots[0].x=NAN;if(mode==1)c.stroke.config.size=-1;if(mode==2)c.stroke.knots.clear();if(mode==3)c.stroke.knots.resize(262145);x.command=CompactStrokeReferenceCommand{c};}
  else if(mode<8){auto& g=std::get<semantic::SweptCircleMask>(x.record.eraseMasks[0].geometry);if(mode==4)g.segments[0].control1.x=NAN;if(mode==5)g.segments[0].p0.radius=-0.;if(mode==6)g.segments.clear();if(mode==7)g.segments[0].p1.radius=-1;}
  else if(mode==8)x.record.transform.tx=NAN;else x.record.visualBounds.left=NAN;
  require(!compare(changedEntry(plan,x)),"invalid content/mask/frame accepted");++rejected;
 }
 // Active masks are never cached even when historical proof hits.
 auto active=e;active.presentationEraseMasks={e.record.eraseMasks[0]};auto activePlan=changedEntry(plan,active);
 metrics::counts={};for(unsigned i=0;i<2;++i)require(validateReferencePlan(activePlan,&cache,HistoryValidationPolicy::kPublishedImmutable).code==BackendSubmissionCode::kAccepted,"active positive");require(metrics::counts.activeSegments==256&&metrics::counts.historySegments==0,"active/historical work conflated");
 std::get<semantic::SweptCircleMask>(active.presentationEraseMasks[0].geometry).segments[0].control1.x=NAN;require(!compare(changedEntry(plan,active)),"invalid active trusted fromhistoryhit");
 // Legacy/custom history routes always recheck even if this identity has a
 // proof from the immutable-plan domain. No own-pin/refcount self-promotion.
 metrics::counts={};for(unsigned i=0;i<2;++i)require(validateReferencePlan(plan,&cache,HistoryValidationPolicy::kUntrusted).code==BackendSubmissionCode::kAccepted,"untrusted positive");require(metrics::counts.maskArrays==2&&metrics::counts.historyHits==0,"untrusted history reused proof");
 // Lawful builder writes after proof pin detach; the old plan remains valid.
 auto before=e.record.eraseMasks.storage();auto cow=e;std::get<semantic::SweptCircleMask>(cow.record.eraseMasks[0].geometry).segments[0].p0.radius=-1;
 require(cow.record.eraseMasks.storage()!=before&&std::get<semantic::SweptCircleMask>(std::as_const(e.record.eraseMasks)[0].geometry).segments[0].p0.radius==1,"proof pin did not preserve lawful COW version");require(compare(plan)&&!compare(changedEntry(plan,cow)),"COW invalidation parity");
 cache.clear();require(budget->used()==0,"proof source/metadata leaked after clear");
 return {{"negative_cases",rejected},{"active_always_checked",true},{"untrusted_legacy_always_checked",true},{"body_history_independent",true},{"legal_COW_old_view_preserved",true},{"code_message_parity",true},{"clear_releases_all",true}};
}
nlohmann::json validationPressure(){
 auto plan=validationPlan(1);auto pool=std::make_shared<SourceRetentionPool>();ReferenceValidationCache cache;RenderCacheOptions options;options.maxEntries=4;
 auto budget=std::make_shared<resources::ResourceBudgetManager>();cache.bind(options,budget,pool);
 for(unsigned i=0;i<8;++i){auto e=plan.referenceDrawList.entries.front();auto c=std::get<CompactStrokeReferenceCommand>(e.command).content.get();c.stroke.seed+=i;e.command=CompactStrokeReferenceCommand{c};require(validateReferencePlan(changedEntry(plan,e),&cache,HistoryValidationPolicy::kPublishedImmutable).code==BackendSubmissionCode::kAccepted,"churn denied correctness");require(cache.size()<=options.maxEntries,"proof entries unbounded");}
 const auto used=budget->used();require(used>cache.metadataBytes(),"proof strong sources uncharged");cache.clear();require(budget->used()==0,"churn release leaked");
 auto tiny=std::make_shared<resources::ResourceBudgetManager>(resources::BudgetLimits{1,1});cache.bind(options,tiny,pool);metrics::counts={};for(unsigned i=0;i<2;++i)require(validateReferencePlan(plan,&cache,HistoryValidationPolicy::kPublishedImmutable).code==BackendSubmissionCode::kAccepted,"quota denial changed accepted/retry");require(cache.size()==0&&tiny->used()==0&&metrics::counts.bodyCalls==2&&metrics::counts.maskArrays==2,"quota fallback skippedvalidation/retainedproof");
 cache.bind(options,budget,pool);require(validateReferencePlan(plan,&cache,HistoryValidationPolicy::kPublishedImmutable).code==BackendSubmissionCode::kAccepted,"rebind ready");
 cache.bind(options,tiny,pool);require(budget->used()==0,"budgetownerchangeleak");
 options.enabled=false;cache.bind(options,budget,pool);require(validateReferencePlan(plan,&cache,HistoryValidationPolicy::kPublishedImmutable)==validateReferencePlan(plan),"cacheoff mismatch");
 options.enabled=true;cache.bind(options,budget,pool);validationFailAllocation=true;
 require(validateReferencePlan(plan,&cache,HistoryValidationPolicy::kPublishedImmutable).code==BackendSubmissionCode::kAccepted&&!validationFailAllocation,"optionalallocationfailurechangedvalidation");cache.clear();require(budget->used()==0,"allocationfailureleakedquota");
 return {{"churn_twice_entries",true},{"retained_sources_charged",true},{"optional_quota_denial_fullcheck_accepted",true},{"optional_allocation_failure_fullcheck_accepted",true},{"budget_owner_switch_release",true},{"cacheoff_fullparity",true},{"resident_peak",used}};
}
nlohmann::json validationRaster(){
 auto base=validationPlan(2);auto list=base.referenceDrawList;for(auto& e:list.entries){e.presentationEraseMasks={e.record.eraseMasks[0]};e.presentationOpacity=.6F;}
 nlohmann::json rows=nlohmann::json::array();
 for(unsigned dpr:{1,2,4}){FrameState f{base.frame.viewId,base.frame.camera,base.frame.worldViewport,{256,128,256*dpr,128*dpr,float(dpr),1},base.frame.sceneGeneration,base.frame.sceneReadToken,base.frame.surfaceGeneration,base.frame.metricsGeneration,base.frame.frameId,base.frame.viewportClip};
  ReferenceDrawList draw{f};draw.entries=list.entries;draw.viewportClip={0,0,256,128};draw.entries[1].record.transform={.9,.1,-.05,1,12,0};
  FramePlan plan{f,std::move(draw),{},false,true};
  ValidationTarget actual,reference;actual.resize(256*dpr,128*dpr);reference.resize(256*dpr,128*dpr);
  SkiaRenderer renderer,uncached;RenderCacheOptions off;off.enabled=false;uncached.setRenderCacheOptions(off);
  for(unsigned i=0;i<3;++i){metrics::counts={};require(renderer.renderFrame(actual,plan).code==BackendSubmissionCode::kAccepted,"cached realRasterframe");
   if(i)require(metrics::counts.bodyCalls==0&&metrics::counts.maskArrays==0&&metrics::counts.bodyHits>0&&metrics::counts.historyHits>0,"actual renderer failed proof reuse");}
  require(uncached.renderFrame(reference,plan).code==BackendSubmissionCode::kAccepted,"uncached realRasterframe");
  require(validationPixels(actual.surface)==validationPixels(reference.surface),"cached Raster pixels changed");
  EditingOverlay overlay{plan.frame};require(renderer.renderTransientOverlay(actual,plan,&overlay).code==BackendSubmissionCode::kAccepted,"actual transientroute");
  require(uncached.renderTransientOverlay(reference,plan,&overlay).code==BackendSubmissionCode::kAccepted,"uncachedtransientroute");require(validationPixels(actual.surface)==validationPixels(reference.surface),"transient mask pixels changed");
  auto badEntry=plan.referenceDrawList.entries[0];std::get<semantic::SweptCircleMask>(badEntry.presentationEraseMasks[0].geometry).segments[0].p0.radius=0;auto bad=changedEntry(plan,badEntry);
  const auto presents=actual.presents;require(renderer.renderFrame(actual,bad).code==BackendSubmissionCode::kRejected&&actual.presents==presents,"badactive trusted orpresented");
  rows.push_back({{"DPR",dpr},{"canonical_real_hits",true},{"transient_shared_hits",true},{"pixels_exact",true},{"invalid_active_no_present",true}});
 }
 return rows;
}
nlohmann::json validationNavigation(){
 auto p=validationPlan(1);auto e=p.referenceDrawList.entries.front();ValidationTarget target;target.resize(256,128);
 SkiaRenderer renderer;RenderCacheOptions options;options.navigationMaxQueries=1;options.navigationMaxSteps=1;options.navigationMicroseconds=1000000;renderer.setRenderCacheOptions(options);
 NavigationSourceQuery stamped=[&](const WorldRect&,std::vector<ReferenceTraversalEntry>& out,NavigationSourceDiagnostics& d){out={e};d.sceneStamped=true;d.semanticGeneration=1;d.sceneRevision=1;return true;};
 metrics::counts={};
 for(unsigned i=0;i<3;++i){auto r=renderer.renderNavigation(target,{1,float(-256*int(i)),0},p.frame,stamped,false,nullptr,{},&p);require(r.code!=BackendSubmissionCode::kRejected,"stamped navigation rejected");}
 const auto hits=metrics::counts.historyHits;require(hits>0&&metrics::counts.bodyHits>0,"actual nav never shared proofdomain");
 // Current composition remains qualified, but custom query sources do not
 // inherit its history eligibility. Their same array is explicitly fullchecked.
 NavigationSourceQuery legacy=[&](const WorldRect&,std::vector<ReferenceTraversalEntry>& out){out={e};return true;};
 renderer.cancelNavigationWork();metrics::counts={};
 for(unsigned i=0;i<3;++i)(void)renderer.renderNavigation(target,{1,float(-2048-256*int(i)),0},p.frame,legacy);
 require(metrics::counts.maskArrays>0,"unstamped actual query skipped historical validation");
 return {{"stamped_actual_history_hits",hits},{"legacy_full_arrays",metrics::counts.maskArrays},{"private_route_policy_preserved",true}};
}
nlohmann::json validationGpuRetry(){
#if defined(__APPLE__)
 auto pool=message(message(reinterpret_cast<id>(objc_getClass("NSAutoreleasePool")),"alloc"),"init");
 const auto device=MTLCreateSystemDefaultDevice();const auto queue=message(device,"newCommandQueue");GrMtlBackendContext backend{sk_cfp<GrMTLHandle>(device),sk_cfp<GrMTLHandle>(queue)};
 auto completion=std::make_shared<NativeMetalCompletion>();completion->context=GrDirectContexts::MakeMetal(backend);require(bool(completion->context),"validation GPUcontext");
 auto e=validationFixture();auto plan=renderPlan(e,1,1);NativeMetalTarget target(completion);target.resize(256,128);SkiaRenderer renderer;
 RenderPreparationBudget one;one.maxCoverageSteps=1;one.maxMicroseconds=1000000;unsigned retries=0;metrics::counts={};
 for(unsigned i=0;;++i){auto result=renderer.renderFrame(target,plan,nullptr,&one);if(result.code==BackendSubmissionCode::kAccepted)break;require(result.code==BackendSubmissionCode::kRetry&&i<12000,"GPUvalidationretryfailed");++retries;require(target.presentCount()==0,"retryfakepresent");}
 require(retries>=2&&metrics::counts.bodyHits>1&&metrics::counts.historyHits>1,"canonicalretrynotsharedproof");
 const auto body=metrics::counts.bodyCalls,history=metrics::counts.maskArrays;require(body==1&&history==1,"GPUretryrescanimmutablecontent");
 auto active=e;active.presentationEraseActive=true;active.presentationEraseMasks={std::as_const(e.record.eraseMasks)[0]};auto transient=changedEntry(plan,active);EditingOverlay overlay{transient.frame};unsigned attempts=0;
 for(unsigned i=0;i<3;++i){auto result=renderer.renderTransientOverlay(target,transient,&overlay);++attempts;require(result.code!=BackendSubmissionCode::kRejected,"transientretryrejected");}
 renderer.serviceResourceRetirements(ResourceServiceMode::kBeginSurfaceTransition);metrics::counts={};(void)renderer.renderFrame(target,plan,nullptr,&one);require(metrics::counts.bodyCalls>=1&&metrics::counts.maskArrays>=1,"surfacecontextresetkeptoldproof");
 renderer.clearRenderCache();completion->context->flushAndSubmit(GrSyncCpu::kYes);message(pool,"drain");
 return {{"canonical_retries",retries},{"body_full",body},{"history_full",history},{"transient_attempts",attempts},{"surface_transition_revalidates",true}};
#else
 return {{"Metal","unavailable"}};
#endif
}
nlohmann::json validationWarmPressure(){
 auto p=validationPlan(1);nlohmann::json rows=nlohmann::json::array();
 for(bool worker:{false,true}){
  ValidationTarget target;target.resize(256,128);SkiaRenderer renderer;RenderCacheOptions options;options.maxBytes=2*1024*1024;options.sharedMaxBytes=4*1024*1024;renderer.setRenderCacheOptions(options);EditingOverlay overlay{p.frame};
  require(renderer.renderTransientOverlay(target,p,&overlay).code==BackendSubmissionCode::kAccepted,"warm proof setup");auto budget=transientDisplayBudget.lock();require(bool(budget),"existingtestbudgetwitness");
  const auto before=budget->used();const auto request=options.sharedMaxBytes-before+1;metrics::counts={};
  if(worker){const auto grant=renderer.reserveCpuWorkerGrant(1,request);require(grant!=0,"WARM_PROOF_BLOCKS_MANDATORY_WORKER_GRANT");require(renderer.releaseCpuWorkerGrant(1,grant),"warmWorkerrelease");}
  else {auto metadata=renderer.reserveCpuPendingMetadata(request);require(bool(metadata),"WARM_PROOF_BLOCKS_MANDATORY_PENDING_METADATA");}
  require(metrics::counts.evictions>0,"mandatoryreclaimdidnotreleaseproof");rows.push_back({{"worker_grant",worker},{"warm_used",before},{"request",request},{"proof_evictions",metrics::counts.evictions},{"success",true}});
 }
 return rows;
}
nlohmann::json validationWarmTicket(){
#if defined(__APPLE__)
 auto pool=message(message(reinterpret_cast<id>(objc_getClass("NSAutoreleasePool")),"alloc"),"init");
 const auto device=MTLCreateSystemDefaultDevice();const auto queue=message(device,"newCommandQueue");GrMtlBackendContext backend{sk_cfp<GrMTLHandle>(device),sk_cfp<GrMTLHandle>(queue)};
 auto completion=std::make_shared<NativeMetalCompletion>();completion->context=GrDirectContexts::MakeMetal(backend);require(bool(completion->context),"ticketMetal");
 auto p=renderPlan(validationFixture(),1,1);NativeMetalTarget target(completion);target.resize(256,128);SkiaRenderer renderer;RenderCacheOptions options;options.maxBytes=4*1024*1024;options.sharedMaxBytes=8*1024*1024;renderer.setRenderCacheOptions(options);EditingOverlay overlay{p.frame};
 for(unsigned i=0;;++i){const auto result=renderer.renderTransientOverlay(target,p,&overlay);completion->context->flushAndSubmit(GrSyncCpu::kYes);renderer.serviceResourceRetirements();if(result.code==BackendSubmissionCode::kAccepted)break;require(result.code==BackendSubmissionCode::kRetry&&i<2048,"warm ticket setup");}
 auto budget=transientDisplayBudget.lock();require(bool(budget),"ticketbudget");const auto used=budget->used();auto pressure=budget->reserve(resources::ResourceClass::kRecorded,options.sharedMaxBytes-used-1,true);require(bool(pressure),"ticketpressureadmission");
 metrics::counts={};const auto result=renderer.renderTransientOverlay(target,p,&overlay);require(metrics::counts.evictions>=2,"ticketfailedwithoutproofreclaim");require(result.message!="GPU completion metadata pending","WARM_PROOF_BLOCKS_MANDATORY_GPU_TICKET");
 budget->release(*pressure);renderer.clearRenderCache();completion->context->flushAndSubmit(GrSyncCpu::kYes);message(pool,"drain");
 return {{"warm_used",used},{"proof_evictions",metrics::counts.evictions},{"result_code",unsigned(result.code)},{"result_message",result.message},{"ticket_denial_recovered",true}};
#else
 return {};
#endif
}
nlohmann::json validationIdentityAndWhitelist(){
 auto p=validationPlan(1);ReferenceValidationCache cache;auto budget=std::make_shared<resources::ResourceBudgetManager>();auto pool=std::make_shared<SourceRetentionPool>();RenderCacheOptions options;cache.bind(options,budget,pool);
 semantic::VectorStrokeContent vector;vector.stroke.brush.nominal_size=8;vector.stroke.brush.opacity=.7;vector.stroke.data=semantic::VectorStrokeData{{{{10,10},.5F,{}},{{20,20},.5F,{}}}};
 semantic::DabStrokeContent dab;dab.stroke.brush.opacity=.7;dab.stroke.data=semantic::DabStrokeData{{{{10,10},8,0,.5F}}};
 semantic::BrushStrokeContent brush;brush.stroke.vector_output.outline={{10,10},{20,10},{20,20}};
 semantic::DabBrushStrokeContent dabBrush;dabBrush.stroke.dab_output.dabs={{{10,10},8,0,.5F}};
 std::vector<ReferenceCommand> commands{p.referenceDrawList.entries[0].command,VectorStrokeReferenceCommand{vector},DabStrokeReferenceCommand{dab},BrushStrokeReferenceCommand{brush},DabBrushStrokeReferenceCommand{dabBrush}};
 for(const auto& command:commands){auto e=p.referenceDrawList.entries[0];e.command=command;auto plan=changedEntry(p,e);require(validateReferencePlan(plan)==validateReferencePlan(plan,&cache,HistoryValidationPolicy::kPublishedImmutable),"whitelist first outcome");metrics::counts={};require(validateReferencePlan(plan,&cache,HistoryValidationPolicy::kPublishedImmutable).code==BackendSubmissionCode::kAccepted&&metrics::counts.bodyCalls==0&&metrics::counts.bodyHits==1,"whitelist not fullproof cached");}
 // One control block aliases two different variant subobjects of same type.
 // ImmutableValue::alias must preserve actual pointee in the key.
 using Pair=std::pair<semantic::ObjectContent,semantic::ObjectContent>;auto c=std::get<CompactStrokeReferenceCommand>(commands[0]).content.get();auto bad=c;bad.stroke.knots[0].x=NAN;
 auto pair=std::make_shared<const Pair>(c,bad);
 // Existing ImmutableValue only permits typed member aliases via ObjectContent.
 // The pair's two copies remain distinct content allocations under lawful API;
 // exact owner+pointee discrimination is also checked directly by comparator.
 ImmutableSourceIdentity a{std::shared_ptr<const void>(pair,&pair->first),sourceTypeTag<semantic::CompactStrokeContent>(),commands[0].index()};
 auto b=a;b.storage=std::shared_ptr<const void>(pair,&pair->second);require(ImmutableSourceIdentityLess{}(a,b)||ImmutableSourceIdentityLess{}(b,a),"sameowner differentpointee collapsed");b=a;b.type=sourceTypeTag<semantic::VectorStrokeContent>();require(ImmutableSourceIdentityLess{}(a,b)||ImmutableSourceIdentityLess{}(b,a),"typecollapsed");b=a;b.command++;require(ImmutableSourceIdentityLess{}(a,b),"validatorcommandmodecollapsed");
 auto noStorage=p.referenceDrawList.entries[0];noStorage.command=VectorStrokeReferenceCommand{};metrics::counts={};(void)validateReferencePlan(changedEntry(p,noStorage),&cache,HistoryValidationPolicy::kPublishedImmutable);(void)validateReferencePlan(changedEntry(p,noStorage),&cache,HistoryValidationPolicy::kPublishedImmutable);require(metrics::counts.bodyHits==0,"defaultabsentallocationcached");
 // Derived connector geometry must be validated on every frame.
 auto connector=p.referenceDrawList.entries[0];semantic::ConnectorContent cc;cc.start.value=semantic::FreePointEndpoint{{10,10}};cc.end.value=semantic::FreePointEndpoint{{20,20}};connector.command=ConnectorReferenceCommand{cc};scene::ConnectorGeometry route;route.route={{10,10},{20,20}};connector.record.derivedGeometry=route;auto valid=changedEntry(p,connector);require(validateReferencePlan(valid,&cache,HistoryValidationPolicy::kPublishedImmutable).code==BackendSubmissionCode::kAccepted,"connector initial");
 std::get<scene::ConnectorGeometry>(connector.record.derivedGeometry).route[1].x=NAN;require(validateReferencePlan(changedEntry(p,connector),&cache,HistoryValidationPolicy::kPublishedImmutable)==validateReferencePlan(changedEntry(p,connector)),"derivedroute falseproof");require(validateReferencePlan(changedEntry(p,connector)).code==BackendSubmissionCode::kRejected,"derivedroute not invalid");
 // Layout-dependent text is intentionally outside the source-local proof
 // domain. Same text content can be accepted/rejected when layout changes.
 auto text=p.referenceDrawList.entries[0];semantic::RichTextContent rich;semantic::Paragraph paragraph;semantic::TextRun run;run.text="abc";run.style.font_size=12.5;paragraph.runs.push_back(run);rich.document.paragraphs.push_back(paragraph);
 text.command=RichTextReferenceCommand{rich,std::make_shared<text::TextLayoutSnapshot>()};require(validateReferencePlan(changedEntry(p,text),&cache,HistoryValidationPolicy::kPublishedImmutable).code==BackendSubmissionCode::kAccepted,"layoutpositive");
 std::get<RichTextReferenceCommand>(text.command).layout.reset();auto textPlan=changedEntry(p,text);require(validateReferencePlan(textPlan,&cache,HistoryValidationPolicy::kPublishedImmutable)==validateReferencePlan(textPlan)&&validateReferencePlan(textPlan).code==BackendSubmissionCode::kRejected,"textlayoutmemoized");
 // Per-request bounds, transform/clip/background/list identity remain checked
 // even after both source domains are warm.
 for(unsigned mode=0;mode<4;++mode){auto list=p.referenceDrawList;foundation::DocumentBackground bg=p.background;
  if(mode==0)list.worldToView.a=NAN;if(mode==1)list.viewportClip.right=NAN;if(mode==2)bg.color[0]=NAN;
  if(mode==3)list.entries[0].record.transform.a=INFINITY;
  FramePlan badPlan{p.frame,std::move(list),bg};require(validateReferencePlan(badPlan,&cache,HistoryValidationPolicy::kPublishedImmutable)==validateReferencePlan(badPlan)&&validateReferencePlan(badPlan).code==BackendSubmissionCode::kRejected,"perrequestvalidationfalseproof");
 }
 // Retain an old version through append/remove/reorder, then revisit both.
 auto e=p.referenceDrawList.entries[0];auto old=e.record.eraseMasks.storage();auto appended=e;appended.record.eraseMasks.push_back(std::as_const(e.record.eraseMasks)[0]);auto reordered=appended;std::swap(reordered.record.eraseMasks[0],reordered.record.eraseMasks[1]);auto removed=reordered;removed.record.eraseMasks.pop_back();
 for(const auto& entry:{e,appended,reordered,removed,e}){auto plan=changedEntry(p,entry);require(validateReferencePlan(plan,&cache,HistoryValidationPolicy::kPublishedImmutable)==validateReferencePlan(plan),"historyappendremovereorderoldversionparity");}
 require(e.record.eraseMasks.storage()==old,"oldhistoryversionchanged");
 return {{"five_content_whitelist",true},{"ownership_pointee_type_mode_key",true},{"no_storage_not_memoized",true},{"derivedConnector_not_memoized",true},{"text_layout_not_memoized",true},{"perrequest_frame_checks",true},{"append_remove_reorder_oldversion",true}};
}
std::vector<std::uint8_t> validationResponse(const semantic::EraseMaskGeometry& geometry,const std::array<std::uint64_t,8>& identity){
 auto path=cpuExactPath(geometry);require(bool(path),"validresponsepath");auto data=path->serialize();std::vector<std::uint8_t> response(128+data->size());
 const auto set=[&](unsigned at,std::uint64_t word,unsigned bytes){for(unsigned i=0;i<bytes;++i)response[at+i]=std::uint8_t(word>>(8*i));};
 set(0,0x31525043,4);set(4,1,4);const auto build=cpuKernelId();std::copy(build.begin(),build.end(),response.begin()+16);for(unsigned i=0;i<8;++i)set(32+i*8,identity[i],8);set(96,data->size(),4);set(100,2,4);std::copy_n(static_cast<const std::uint8_t*>(data->data()),data->size(),response.begin()+128);return response;
}
nlohmann::json validationRemainingMandatory(const std::string& mode){
 auto p=validationPlan(1);ValidationTarget target;target.resize(256,128);SkiaRenderer renderer;RenderCacheOptions options;options.maxBytes=16U*1024U*1024U;options.sharedMaxBytes=24U*1024U*1024U;renderer.setRenderCacheOptions(options);EditingOverlay overlay{p.frame};
 require(renderer.renderTransientOverlay(target,p,&overlay).code==BackendSubmissionCode::kAccepted,"remaining warmsetup");auto budget=transientDisplayBudget.lock();require(bool(budget),"remaining budgetwitness");
 const auto& geometry=std::as_const(p.referenceDrawList.entries[0].record.eraseMasks)[0].geometry;
 const std::array<std::uint64_t,8> identity{1,1,2,0,1,1,1,1};const auto response=validationResponse(geometry,identity);
 SkiaRenderer::CpuPathPin pin;SelectionGeometryHit confirmControl=SelectionGeometryHit::kInvalid,confirmResult=SelectionGeometryHit::kInvalid;
 auto confirmedRecord=p.referenceDrawList.entries[0].record;confirmedRecord.kind=semantic::ObjectKind::kVectorStroke;confirmedRecord.eraseMasks.clear();
 if(mode=="confirm"){
  pin=renderer.importCpuPendingPath(geometry,response,identity);require(bool(pin),"confirmvalidpinsetup");
  confirmControl=renderer.confirmCpuPendingErase(confirmedRecord,geometry,pin);require(confirmControl==SelectionGeometryHit::kHit||confirmControl==SelectionGeometryHit::kMiss,"confirm without pressure control must be a decision");
 }
 // The import peak includes its 8MiB precharge AND a live published path.
 // Measure that existing resident charge using an independent owner; merely
 // leaving 8MiB-1 free would still be insufficient after all proofs retire.
 std::size_t importedResident=0,residentBefore=0,residentAfter=0;
 if(mode=="import"){
  ValidationTarget probeTarget;probeTarget.resize(256,128);SkiaRenderer probe;probe.setRenderCacheOptions(options);
  require(probe.renderTransientOverlay(probeTarget,p,&overlay).code==BackendSubmissionCode::kAccepted,"import capacity probe");
  auto probeBudget=transientDisplayBudget.lock();residentBefore=probeBudget->used();metrics::counts={};
  auto probePin=probe.importCpuPendingPath(geometry,response,identity);residentAfter=probeBudget->used();require(bool(probePin)&&residentAfter>=residentBefore&&metrics::counts.evictions==0,"import capacity control");importedResident=residentAfter-residentBefore;
  probePin.reset();require(probeBudget->used()==residentAfter,"published resident must stay held by path cache");
  require(probe.renderTransientOverlay(probeTarget,p,&overlay).code==BackendSubmissionCode::kAccepted&&metrics::counts.bodyCalls==0&&metrics::counts.maskArrays==0,"resident probe changed validation state");
 }
 const auto free=mode=="import"?8U*1024U*1024U+importedResident-1:mode=="confirm"?4095U:1U;
 const auto used=budget->used();require(used+free<options.sharedMaxBytes,"remainingpressurecapacity");auto token=budget->reserve(resources::ResourceClass::kRecorded,options.sharedMaxBytes-used-free,true);require(bool(token),"remainingpressuregrant");
 metrics::counts={};bool success=false;
 if(mode=="handoff"){success=renderer.retainCpuPendingHandoff(42,{});if(success)renderer.retireCpuPendingHandoff(42);}
 if(mode=="import"){bool pending=false;pin=renderer.importCpuPendingPath(geometry,response,identity,&pending);success=bool(pin)&&!pending;}
 if(mode=="confirm"){confirmResult=renderer.confirmCpuPendingErase(confirmedRecord,geometry,pin);success=confirmResult==confirmControl;}
 std::cerr<<nlohmann::json{{"mandatory_probe",mode},{"success",success},{"initial_used",used},{"current_used",budget->used()},{"reclaimed",metrics::counts.evictions},{"free_before",free},{"free_after",options.sharedMaxBytes-budget->used()},{"confirm_control",unsigned(confirmControl)},{"confirm_result",unsigned(confirmResult)},{"resident_before",residentBefore},{"resident_after",residentAfter},{"import_resident",importedResident}}.dump()<<'\n';budget->release(*token);require(success,("WARM_PROOF_BLOCKS_MANDATORY_"+mode).c_str());require(metrics::counts.evictions>0,"remainingproofnotreclaimed");
 return {{"mode",mode},{"success",success},{"warm_used",used},{"proof_evictions",metrics::counts.evictions},{"free_before",free},{"import_resident_control",importedResident},{"resident_before",residentBefore},{"resident_after",residentAfter},{"confirm_control",unsigned(confirmControl)},{"confirm_result",unsigned(confirmResult)}};
}
nlohmann::json validationTransientMetadataPressure(){
 auto p=validationPlan(1);ValidationTarget target;target.resize(256,128);SkiaRenderer renderer;RenderCacheOptions options;options.maxBytes=16U*1024U*1024U;options.sharedMaxBytes=24U*1024U*1024U;renderer.setRenderCacheOptions(options);EditingOverlay overlay{p.frame};
 require(renderer.renderTransientOverlay(target,p,&overlay).code==BackendSubmissionCode::kAccepted,"transient metadata warm setup");
 auto budget=transientDisplayBudget.lock();require(bool(budget),"metadata budget witness");
 require(renderer.renderFrame(target,p).code==BackendSubmissionCode::kAccepted,"canonical retires disposable transient surface");
 const auto used=budget->used();auto pressure=budget->reserve(resources::ResourceClass::kRecorded,options.sharedMaxBytes-used-1,true);require(bool(pressure),"transient metadata pressure");
 const auto beforePixels=validationPixels(target.surface);const auto beforePresents=target.presents;metrics::counts={};
 const auto result=renderer.renderTransientOverlay(target,p,&overlay);const auto evicted=metrics::counts.evictions;
 require(result.message!="transient display metadata pending","WARM_PROOF_BLOCKS_MANDATORY_TRANSIENT_METADATA");
 require(evicted>0,"transient metadata proof not reclaimed");
 require(result.code==BackendSubmissionCode::kRetry&&target.presents==beforePresents&&validationPixels(target.surface)==beforePixels,"transient pressure published an incomplete frame");
 budget->release(*pressure);require(renderer.renderTransientOverlay(target,p,&overlay).code==BackendSubmissionCode::kAccepted,"transient metadata pressure did not recover");
 return {{"warm_used",used},{"proof_evictions",evicted},{"result_after_metadata",result.message},{"pixels_preserved",true},{"receipt_withheld_until_complete",true},{"released_pressure_recovers",true}};
}

nlohmann::json validationPressureSafety(){
 auto p=validationPlan(1);RenderCacheOptions options;options.maxBytes=16U*1024U*1024U;options.sharedMaxBytes=24U*1024U*1024U;
 ValidationTarget target;target.resize(256,128);SkiaRenderer renderer;renderer.setRenderCacheOptions(options);EditingOverlay overlay{p.frame};
 require(renderer.renderTransientOverlay(target,p,&overlay).code==BackendSubmissionCode::kAccepted,"safety warm proof setup");
 auto record=p.referenceDrawList.entries[0].record;record.kind=semantic::ObjectKind::kVectorStroke;record.eraseMasks.clear();
 const auto& geometry=std::as_const(p.referenceDrawList.entries[0].record.eraseMasks)[0].geometry;
 metrics::counts={};auto job=renderer.requestCpuLocalExact(record,geometry);require(bool(job),"no pressure actual exact request");
 renderer.beginCpuPendingOpportunity();const auto first=renderer.advanceCpuLocalExact(job);require(first!=LocalExactStatus::kInvalid,"actual exact advance witness");
 require(metrics::counts.evictions==0,"ordinary request/yield evicted validation proof");
 auto before=job->statistics();const std::array<std::uint64_t,8> identity{1,1,2,0,1,1,1,1};auto response=validationResponse(geometry,identity);bool pending=true;
 auto malformed=response;malformed[0]=0;require(!renderer.importCpuPendingPath(geometry,malformed,identity,&pending)&&!pending,"malformed import accepted/pending");
 auto foreign=identity;foreign[0]++;require(!renderer.importCpuPendingPath(geometry,response,foreign,&pending)&&!pending,"foreign import accepted/pending");
 require(!renderer.importCpuLocalExact(job,response,identity,&pending)&&!pending,"local import without matching request accepted");
 require(job->statistics().validated==before.validated&&metrics::counts.evictions==0,"invalid import reset/advanced job or evicted proof");
 const auto decision=renderer.intersectEraseGeometry(record,geometry);require(decision==SelectionGeometryHit::kHit||decision==SelectionGeometryHit::kMiss,"actual no pressure query control");
 require(metrics::counts.evictions==0,"no pressure query evicted proof");metrics::counts={};
 require(renderer.renderTransientOverlay(target,p,&overlay).code==BackendSubmissionCode::kAccepted&&metrics::counts.bodyCalls==0&&metrics::counts.maskArrays==0,"exact APIs destroyed warm validation reuse");
 // Use the actual shared weak callback to exercise helper and owner lifetimes.
 auto budget=std::make_shared<resources::ResourceBudgetManager>(resources::BudgetLimits{options.maxBytes,options.sharedMaxBytes});
 auto pool=std::make_shared<SourceRetentionPool>();auto proof=std::make_shared<ReferenceValidationCache>();proof->bind(options,budget,pool);
 auto pressure=validationPressure(proof,budget);pool->setValidationPressure(pressure);
 require(validateReferencePlan(p,proof.get(),HistoryValidationPolicy::kPublishedImmutable).code==BackendSubmissionCode::kAccepted,"helper proof setup");
 LocalExactCache exact;exact.setValidationPressure(pressure);const auto initial=budget->used();
 auto held=budget->reserve(resources::ResourceClass::kRecorded,options.sharedMaxBytes-initial-1,true);require(bool(held),"helper actual pressure");metrics::counts={};
 auto index=exact.request(geometry,budget,options.maxEntryBytes);require(bool(index)&&metrics::counts.evictions==2,"actual LocalExact critical reserve did not reclaim proofs");
 budget->release(*held);index.reset();exact.clear();
 require(validateReferencePlan(p,proof.get(),HistoryValidationPolicy::kPublishedImmutable).code==BackendSubmissionCode::kAccepted,"rebind warm setup");
 auto nextBudget=std::make_shared<resources::ResourceBudgetManager>();auto nextPool=std::make_shared<SourceRetentionPool>();proof->bind(RenderCacheOptions{},nextBudget,nextPool);
 require(validateReferencePlan(p,proof.get(),HistoryValidationPolicy::kPublishedImmutable).code==BackendSubmissionCode::kAccepted,"new owner warm setup");
 const auto nextSize=proof->size();pressure(budget);pressure(nextBudget);require(proof->size()==nextSize,"stale helper callback cleared new owner proofs");
 auto afterOwner=validationPressure(proof,nextBudget);proof.reset();afterOwner(nextBudget);pressure(budget);
 // Actual failed reservations preserve parameters and perform a single retry.
 unsigned callbacks=0;auto full=std::make_shared<resources::ResourceBudgetManager>(resources::BudgetLimits{4096,8192});auto occupancy=full->reserve(resources::ResourceClass::kRecorded,8192,true);require(bool(occupancy),"full owner control");
 const ValidationPressure count=[&](const auto& actual){require(actual==full,"retry changed owner");++callbacks;};
 require(!reserveWithSourcePressure(full,count,resources::ResourceClass::kScratch,4096,false,0)&&callbacks==0,"optional admission invoked proof pressure");
 require(!reserveWithSourcePressure(full,count,resources::ResourceClass::kScratch,4096,true,0)&&callbacks==1&&full->used()==8192,"failed retry changed original capacity/failure");full->release(*occupancy);
 auto grant=reserveWithSourcePressure(full,count,resources::ResourceClass::kScratch,4096,true,0);require(bool(grant)&&callbacks==1&&full->snapshot(resources::ResourceClass::kScratch).staging==4096,"successful first admission retried or changed residency");full->release(*grant);
 // A yield uses the same admitted job and the caller's exhausted allowance.
 auto localBudget=std::make_shared<resources::ResourceBudgetManager>();LocalExactCache local;PreparedGeometryCache bodies;auto query=local.request(geometry,localBudget,options.maxEntryBytes);LocalExactJob resumed(record,query,local,bodies,options,localBudget);
 RenderPreparationBudget one;one.maxCoverageSteps=1;one.maxMicroseconds=1000000;PreparationAllowance slice(one);require(resumed.advance(slice)==LocalExactStatus::kPending&&slice.steps==1,"one-step exact yield");
 const auto work=resumed.statistics();require(resumed.advance(slice)==LocalExactStatus::kPending&&slice.steps==1&&resumed.statistics().validated==work.validated,"same opportunity reset allowance/progress");
 PreparationAllowance next(one);require(resumed.advance(next)==LocalExactStatus::kPending&&next.steps==1&&resumed.statistics().validated>work.validated,"next opportunity did not resume existing cursor");
 // Prepared cold jobs reclaim optional proofs only on a failed critical
 // source/index/replay reservation; all subsequent advances reuse that body.
 auto prepareBudget=std::make_shared<resources::ResourceBudgetManager>(resources::BudgetLimits{options.maxBytes,options.sharedMaxBytes});auto preparePool=std::make_shared<SourceRetentionPool>();auto prepareProof=std::make_shared<ReferenceValidationCache>();prepareProof->bind(options,prepareBudget,preparePool);
 require(validateReferencePlan(p,prepareProof.get(),HistoryValidationPolicy::kPublishedImmutable).code==BackendSubmissionCode::kAccepted,"prepared pressure warm proofs");
 auto hold=prepareBudget->reserve(resources::ResourceClass::kRecorded,options.sharedMaxBytes-prepareBudget->used()-1,true);require(bool(hold),"prepared pressure occupancy");PreparedGeometryCache prepared;prepared.setRetentionPool(preparePool);auto reclaim=validationPressure(prepareProof,prepareBudget);preparePool->setValidationPressure(reclaim);prepared.setValidationPressure(reclaim);metrics::counts={};
 require(!prepared.request(p.referenceDrawList.entries[0],options,prepareBudget)&&metrics::counts.evictions==2,"mandatory prepared reservation retained optional proofs");prepareBudget->release(*hold);
 auto resumable=prepared.request(p.referenceDrawList.entries[0],options,prepareBudget);require(bool(resumable),"prepared cold pressure release recovery");const auto admissions=prepared.statistics().jobAdmissions;
 for(unsigned i=0;!resumable->ready();++i){PreparationAllowance quantum(one);prepared.advance(resumable,quantum);require(quantum.steps==1&&!resumable->failed()&&i<4096,"prepared pressure job lost bounded progress");}
 require(prepared.statistics().jobAdmissions==admissions&&prepared.statistics().compileInputs==8192,"prepared job recreated/reset after reserve recovery");
 return {{"actual_renderer_exact_no_pressure_proof_reuse",true},{"yield_and_invalid_import_no_eviction",true},{"actual_LocalExact_pressure_reclaims",true},{"stale_budget_callback_no_wrong_clear",true},{"destroyed_owner_callback_safe",true},{"optional_no_callback",true},{"failed_identical_retry_preserves_failure",true},{"same_job_cursor_allowance_preserved",true},{"prepared_job_pressure_and_bounded_resume",true}};
}

nlohmann::json validationGpuAdmissionSafety(){
 struct Completion final:GpuCompletionService {
  std::shared_ptr<GpuCompletionState> flags=std::make_shared<GpuCompletionState>();
  void service()noexcept override{}
  std::shared_ptr<const GpuCompletionState> state()const noexcept override{return flags;}
 };
 auto p=validationPlan(1);auto budget=std::make_shared<resources::ResourceBudgetManager>();auto sources=std::make_shared<SourceRetentionPool>();auto proof=std::make_shared<ReferenceValidationCache>();proof->bind(RenderCacheOptions{},budget,sources);
 require(validateReferencePlan(p,proof.get(),HistoryValidationPolicy::kPublishedImmutable).code==BackendSubmissionCode::kAccepted,"GPU proof control");
 unsigned calls=0;auto weakPressure=validationPressure(proof,budget);const ValidationPressure pressure=[&](const auto& owner){++calls;weakPressure(owner);};
 auto service=std::make_shared<Completion>();GpuResourceRetirement queue;
 require(!queue.begin(budget,{},resources::kPixelResourceClasses,resources::kPixelResourceClasses,pressure)&&calls==0&&proof->size()==2,"missing GPU service evicted proof");
 auto epoch=budget->beginGpuEpoch();require(bool(epoch),"epoch conflict control");
 require(!queue.begin(budget,service,resources::kPixelResourceClasses,resources::kPixelResourceClasses,pressure)&&calls==0&&proof->size()==2,"epoch conflict evicted proof/retried submission");
 require(budget->endGpuEpoch(*epoch)&&budget->completeGpuEpoch(*epoch),"epoch conflict cleanup");
 const auto used=budget->used();auto held=budget->reserve(resources::ResourceClass::kRecorded,96U*1024U*1024U-used-1,true);require(bool(held),"GPU reserve pressure control");metrics::counts={};
 auto ticket=queue.begin(budget,service,resources::kPixelResourceClasses,resources::kPixelResourceClasses,pressure);require(bool(ticket)&&calls==1&&metrics::counts.evictions==2&&budget->pendingGpuEpochCount()==1,"GPU actual reserve retry not single admission");
 const auto admitted=ticket->epoch;ticket->end();ticket->unused();queue.service();require(budget->pendingGpuEpochCount()==0&&budget->gpuEpochRetired(admitted),"GPU recovered epoch did not retire once");ticket.reset();budget->release(*held);require(budget->used()==0,"GPU recovered ticket/source quota leaked");
 // Saturation is checked before admission and must not reclaim optional proof.
 require(validateReferencePlan(p,proof.get(),HistoryValidationPolicy::kPublishedImmutable).code==BackendSubmissionCode::kAccepted,"saturation proof control");
 std::vector<std::shared_ptr<GpuResourceRetirement::Ticket>> tickets;
 for(unsigned i=0;i<1024;++i){auto t=queue.begin(budget,service,resources::kPixelResourceClasses,resources::kPixelResourceClasses,pressure);require(bool(t),"queue saturation control");t->end();tickets.push_back(std::move(t));}
 const auto callbacks=calls;require(!queue.begin(budget,service,resources::kPixelResourceClasses,resources::kPixelResourceClasses,pressure)&&calls==callbacks&&proof->size()==2,"queue capacity evicted proof/retried begin");
 for(const auto& t:tickets)t->unused();queue.service();tickets.clear();require(budget->pendingGpuEpochCount()==0,"saturated queue cleanup");proof->clear();require(budget->used()==0,"GPU admission test leaked");
 return {{"actual_reserve_failure_callback_count",1},{"one_epoch_one_retirement",true},{"absent_completion_no_reclaim",true},{"epoch_conflict_no_reclaim",true},{"queue_saturation_no_reclaim",true},{"no_submission_on_failed_admission",true}};
}

nlohmann::json validationCoverageLayerPressure(){
 auto p=validationPlan(1);RenderCacheOptions options;options.maxBytes=16U*1024U*1024U;options.sharedMaxBytes=24U*1024U*1024U;
 auto budget=std::make_shared<resources::ResourceBudgetManager>(resources::BudgetLimits{options.maxBytes,options.sharedMaxBytes});auto pool=std::make_shared<SourceRetentionPool>();auto proof=std::make_shared<ReferenceValidationCache>();proof->bind(options,budget,pool);
 require(validateReferencePlan(p,proof.get(),HistoryValidationPolicy::kPublishedImmutable).code==BackendSubmissionCode::kAccepted,"coverage proof setup");
 auto target=SkSurfaces::Raster(SkImageInfo::MakeN32Premul(64,64));CompactCoverageTargets targets;targets.budget=budget;unsigned callbacks=0;const auto weak=validationPressure(proof,budget);const ValidationPressure pressure=[&](const auto& owner){++callbacks;weak(owner);};targets.setValidationPressure(pressure);
 auto held=budget->reserve(resources::ResourceClass::kRecorded,options.sharedMaxBytes-budget->used()-1,true);require(bool(held),"coverage pressure control");metrics::counts={};CompactRasterWork work;
 auto pixels=targets.acquire(*target->getCanvas(),8,8,false,&work);require(bool(pixels)&&callbacks==1&&metrics::counts.evictions==2,"actual coverage reserve blocked by optional proof");
 require(targets.acquire(*target->getCanvas(),8,8,false,&work)==pixels&&callbacks==1,"coverage slot reuse invoked callback");
 require(!targets.acquire(*target->getCanvas(),0,8,false,&work)&&callbacks==1,"invalid coverage dimensions reclaimed proof");
 ExecutionDecision wrong;wrong.route=ExecutionRoute::kGpu;wrong.context=reinterpret_cast<GrRecordingContext*>(std::uintptr_t(1));
 require(!targets.acquire(*target->getCanvas(),8,8,wrong,&work).surface&&callbacks==1,"wrong coverage context reclaimed proof");
 budget->release(*held);targets.clear();pixels.reset();proof->clear();require(budget->used()==0,"coverage pressure lease leak");
 nlohmann::json layerRows=nlohmann::json::array();
 for(bool metadataFirst:{true,false}){
  auto layerBudget=std::make_shared<resources::ResourceBudgetManager>(resources::BudgetLimits{options.maxBytes,options.sharedMaxBytes});auto sources=std::make_shared<SourceRetentionPool>();auto layerProof=std::make_shared<ReferenceValidationCache>();layerProof->bind(options,layerBudget,sources);
  auto e=p.referenceDrawList.entries[0];auto c=std::get<CompactStrokeReferenceCommand>(e.command).content.get();c.stroke.knots={{8,8,32768},{24,24,32768}};e.record.content=c;e.command=CompactStrokeReferenceCommand{scene::ImmutableValue<semantic::CompactStrokeContent>::alias(e.record.content)};e.record.eraseMasks.clear();e.presentationEraseActive=true;
  auto m=std::as_const(p.referenceDrawList.entries[0].record.eraseMasks)[0];std::get<semantic::SweptCircleMask>(m.geometry).segments.resize(1);e.presentationEraseMasks={m};
  auto geometry=std::make_shared<PreparedGeometryCache>();geometry->setRetentionPool(sources);auto readyBody=geometry->request(e,options,layerBudget);require(bool(readyBody),"layer prepared setup");
  RenderPreparationBudget quantum;quantum.maxCoverageSteps=1;quantum.maxMicroseconds=1000000;
  while(!readyBody->ready()){PreparationAllowance a(quantum);geometry->advance(readyBody,a);require(!readyBody->failed(),"layer prepared setup failed");}
  // A separate proven immutable allocation provides disposable quota; the
  // currently drawn source/history stay pinned by their original owners.
  require(validateReferencePlan(p,layerProof.get(),HistoryValidationPolicy::kPublishedImmutable).code==BackendSubmissionCode::kAccepted,"layer proof control");
  SkiaInkLayers layers;CompactCoverageTargets coverage;coverage.budget=layerBudget;unsigned invokes=0;const auto weakLayer=validationPressure(layerProof,layerBudget);const ValidationPressure hook=[&](const auto& owner){++invokes;weakLayer(owner);};layers.setValidationPressure(hook);coverage.setValidationPressure(hook);
  auto scratch=SkSurfaces::Raster(SkImageInfo::MakeN32Premul(32,32)),destination=SkSurfaces::Raster(SkImageInfo::MakeN32Premul(32,32));RenderCacheWork counters;bool painted=false,pending=false;
  const auto free=metadataFirst?1U:4096U;
  auto occupancy=layerBudget->reserve(resources::ResourceClass::kRecorded,options.sharedMaxBytes-layerBudget->used()-free,true);require(bool(occupancy),"layer occupancy");metrics::counts={};
  const bool complete=layers.draw(*destination->getCanvas(),*scratch,e,SkMatrix::I(),SkIRect::MakeWH(32,32),nullptr,options,layerBudget,sources,coverage,counters,painted,pending,geometry,readyBody);
  require(invokes>0&&metrics::counts.evictions==2,"layer critical reserve missed proof reclamation");
  // The callback only frees optional proof metadata; an insufficient budget
  // remains pending. It does not fabricate a complete frame.
  if(!complete)require(pending,"layer failure lost original resource pending");
  layerBudget->release(*occupancy);bool ready=complete;
  for(unsigned i=0;!ready&&i<4096;++i)ready=layers.draw(*destination->getCanvas(),*scratch,e,SkMatrix::I(),SkIRect::MakeWH(32,32),nullptr,options,layerBudget,sources,coverage,counters,painted,pending,geometry,readyBody);
  require(ready,"layer pressure release did not recover");
  const auto beforeCallbacks=invokes;const auto localSize=options.maxEntries;options.maxEntries=0;
  require(!layers.draw(*destination->getCanvas(),*scratch,e,SkMatrix::I(),SkIRect::MakeWH(32,32),nullptr,options,layerBudget,sources,coverage,counters,painted,pending,geometry,readyBody)&&invokes==beforeCallbacks,"local layer quota invoked pressure hook");options.maxEntries=localSize;
  require(validateReferencePlan(p,layerProof.get(),HistoryValidationPolicy::kPublishedImmutable).code==BackendSubmissionCode::kAccepted,"layer rewarm proof");metrics::counts={};
  require(layers.draw(*destination->getCanvas(),*scratch,e,SkMatrix::I(),SkIRect::MakeWH(32,32),nullptr,options,layerBudget,sources,coverage,counters,painted,pending,geometry,readyBody)&&invokes==beforeCallbacks&&metrics::counts.evictions==0,"no-pressure layer draw reclaimed proof");
  layerRows.push_back({{"metadata_first",metadataFirst},{"callbacks",invokes},{"actual_draw_completed_after_release",true},{"initial_complete",complete}});
 }
 return {{"coverage_actual_denial_recovers",true},{"slot_reuse_structural_context_refusal_no_callback",true},{"layers",layerRows}};
}

}
int main(int argc,char**argv){try{
 if(argc==3&&std::string_view(argv[1])=="--remaining-mandatory"){std::cout<<validationRemainingMandatory(argv[2]).dump()<<'\n';return 0;}
 if(argc==2&&std::string_view(argv[1])=="--coverage-layer-pressure"){std::cout<<validationCoverageLayerPressure().dump()<<'\n';return 0;}
 if(argc==2&&std::string_view(argv[1])=="--gpu-admission-safety"){std::cout<<validationGpuAdmissionSafety().dump()<<'\n';return 0;}
 if(argc==2&&std::string_view(argv[1])=="--pressure-safety"){std::cout<<validationPressureSafety().dump()<<'\n';return 0;}
 if(argc==2&&std::string_view(argv[1])=="--transient-metadata-pressure"){std::cout<<validationTransientMetadataPressure().dump()<<'\n';return 0;}
 if(argc==2&&std::string_view(argv[1])=="--warm-pressure"){std::cout<<validationWarmPressure().dump()<<'\n';return 0;}
 if(argc==2&&std::string_view(argv[1])=="--ticket-pressure"){std::cout<<validationWarmTicket().dump()<<'\n';return 0;}
 require(argc==2,"validation output");namespace m=foundation::internal::reference_validation_metrics;auto plan=validationPlan(16);m::counts={};
 ReferenceValidationCache cache;auto pool=std::make_shared<SourceRetentionPool>();auto budget=std::make_shared<resources::ResourceBudgetManager>();cache.bind(RenderCacheOptions{},budget,pool);
 nlohmann::json validationMicros=nlohmann::json::array();
 for(unsigned i=0;i<32;++i){const auto began=std::chrono::steady_clock::now();require(validateReferencePlan(plan,&cache,HistoryValidationPolicy::kPublishedImmutable).code==BackendSubmissionCode::kAccepted,"valid fixture rejected");validationMicros.push_back(std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-began).count());}
 const auto work=m::counts;nlohmann::json result{{"copies",16},{"calls",32},{"knots",8192},{"history_segments",1024},{"history_path_commands",128},{"body_full_calls",work.bodyCalls},{"requested_knots",work.compactRequestedKnots},{"history_arrays",work.maskArrays},{"visited_mask_segments",work.maskSegments},{"visited_mask_path_commands",work.maskPathCommands}};
 const bool once=work.bodyCalls==1&&work.maskArrays==1;result["once_per_immutable_allocation"]=once;
 result["validation_us_cold_then31reuse"]=validationMicros;result["timing_qualification"]="Native fullvalidator plus lookup/sourceadmission/framechecks; first includescoldvalidation, retainedfootprint and allocation; hot includesperrequestframechecks. Not browserowner/GPUtime.";
 result["proof_lookup_comparisons"]=work.identityProbes;result["metadata_bytes"]=cache.metadataBytes();result["all_retained_accounted_budget_bytes"]=budget->used();
 result["negatives"]=validationNegatives();result["pressure"]=validationPressure();result["Raster"]=validationRaster();result["navigation"]=validationNavigation();result["GPU_retry"]=validationGpuRetry();result["warm_pressure"]=validationWarmPressure();result["warm_ticket"]=validationWarmTicket();result["identity"]=validationIdentityAndWhitelist();
 std::ofstream(argv[1])<<result.dump(2)<<'\n';std::cout<<result.dump()<<'\n';require(once,"IMMUTABLE_SOURCE_VALIDATION_REPEATED_FULL_SCANS");return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
