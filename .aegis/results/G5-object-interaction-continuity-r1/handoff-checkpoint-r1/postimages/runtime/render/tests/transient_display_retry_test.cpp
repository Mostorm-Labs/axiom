// Real provider pixels, not present counters, are the failure oracle.
#define main g5OriginalCanonicalTestMain
#include "canonical_prepared_body_test.cpp"
#undef main
#include <filesystem>
#include "../src/transient_display_state.hpp"
#include <nlohmann/json.hpp>
#include <iostream>
namespace {
ReferenceTraversalEntry displaySource(){
 auto e=source();e.record.kind=semantic::ObjectKind::kVectorStroke;e.record.kindVersion=3;
 e.record.content.update([](auto& v){auto& c=std::get<semantic::CompactStrokeContent>(v);c.stroke.knots.clear();for(unsigned i=0;i<32;++i)c.stroke.knots.push_back({20.+i*5,64,32768});});
 e.command=CompactStrokeReferenceCommand{scene::ImmutableValue<semantic::CompactStrokeContent>::alias(e.record.content)};
 semantic::SweptCircleMask mask;mask.segments.push_back({{{80,40},2},{{80,80},2},{80,50},{80,70}});e.record.eraseMasks.push_back({ObjectId::fromUint64(500),mask});e.presentationEraseActive=true;
 return e;
}
void saveBuffer(const std::filesystem::path& path,const std::vector<std::uint8_t>& bytes){std::ofstream f(path,std::ios::binary);f.write(reinterpret_cast<const char*>(bytes.data()),bytes.size());require(bool(f),"persist actual target buffer");}
#if defined(__APPLE__)
void failedPixels(const std::filesystem::path& out){
 std::filesystem::create_directories(out);auto device=MTLCreateSystemDefaultDevice(),queue=message(device,"newCommandQueue");auto completion=std::make_shared<NativeMetalCompletion>();GrMtlBackendContext backend{sk_cfp<GrMTLHandle>(device),sk_cfp<GrMTLHandle>(queue)};completion->context=GrDirectContexts::MakeMetal(backend);require(bool(completion->context),"real Metal context");
 for(unsigned kib:{320U,352U,384U,416U,448U,480U,512U,576U,640U,1024U,2048U}){
  auto e=displaySource();NativeMetalTarget target(completion);require(target.resize(256,128).code==BackendSubmissionCode::kAccepted,"target resize");auto acquired=target.acquire();
  auto base=renderPlan(e,1,1);auto draw=base.referenceDrawList;draw.entries={e};FramePlan plan{base.frame,std::move(draw),{},false,true};
  require(drawReferencePlanToSkCanvas(*acquired.frame.surface->getCanvas(),plan).code==BackendSubmissionCode::kAccepted,"initial complete frame");target.release();require(target.present().code==BackendSubmissionCode::kAccepted,"initial accepted present");
  std::vector<std::uint8_t> before(256*128*4),after(before.size());require(target.readbackRgba(before).code==BackendSubmissionCode::kAccepted,"read initial actual target");const auto count=target.presentCount();
  SkiaRenderer renderer;RenderCacheOptions options;options.maxBytes=kib*1024;options.sharedMaxBytes=kib*1024;renderer.setRenderCacheOptions(options);EditingOverlay overlay{plan.frame};
  for(unsigned attempt=0;attempt<8;++attempt){
   const auto status=renderer.renderTransientOverlay(target,plan,&overlay);completion->context->flushAndSubmit(GrSyncCpu::kYes);renderer.serviceResourceRetirements();
   require(target.readbackRgba(after).code==BackendSubmissionCode::kAccepted,"read failed actual target after GPU completion");
   std::size_t changed=0;for(std::size_t p=0;p<before.size();p+=4)if(!std::equal(before.begin()+p,before.begin()+p+4,after.begin()+p))++changed;
   const auto w=renderer.renderCacheWork();const auto code=static_cast<unsigned>(status.code);
   nlohmann::json row{{"budget_kib",kib},{"attempt",attempt},{"status",code},{"message",status.message},{"changed_pixels",changed},{"before_present",count},{"after_present",target.presentCount()},{"generation",target.generation()},{"CPU_tiles",w.cpuCoverageTiles},{"GPU_tiles",w.gpuCoverageTiles},{"body_builds",w.preparedBodyBuilds}};
   std::cout<<row.dump()<<std::endl;
   if(status.code==BackendSubmissionCode::kRetry){
    const auto prefix=out/(std::to_string(kib)+"-"+std::to_string(attempt));saveBuffer(prefix.string()+"-before.rgba",before);saveBuffer(prefix.string()+"-after.rgba",after);std::ofstream(prefix.string()+".json")<<row.dump(2)<<'\n';
    require(count==target.presentCount(),"Retry fabricated present");require(changed==0,"ACTUAL_RENDERER_RETRY_DESTROYED_LAST_ACCEPTED_PIXELS");
   }
   if(status.code!=BackendSubmissionCode::kRetry)break;
  }
 }
}

class DisplayTarget final:public SkiaSurfaceProvider {
 public:
 NativeMetalTarget target;bool unavailable=false,refusePresent=false;
 explicit DisplayTarget(std::shared_ptr<NativeMetalCompletion> c):target(std::move(c)){}
 std::shared_ptr<GpuCompletionService> gpuCompletionService()const noexcept override{return target.gpuCompletionService();}
 RenderTargetInfo describe()const noexcept override{return target.describe();}
 SkiaSurfaceAcquireResult acquire()noexcept override{return unavailable?SkiaSurfaceAcquireResult{}:target.acquire();}
 void release()noexcept override{target.release();}
 BackendSubmissionResult present()noexcept override{return refusePresent?BackendSubmissionResult::rejected("synthetic provider failure after complete publication"):target.present();}
 BackendSubmissionResult resize(std::uint32_t w,std::uint32_t h)noexcept override{return target.resize(w,h);}
 BackendSubmissionResult readbackRgba(std::span<std::uint8_t> out)noexcept override{return target.readbackRgba(out);}
 std::uint64_t generation()const noexcept override{return target.generation();}
 BackendSubmissionResult advanceGeneration()noexcept override{return target.advanceGeneration();}
 std::uint64_t readbackCount()const noexcept override{return target.readbackCount();}
 std::uint64_t cpuCopyCount()const noexcept override{return target.cpuCopyCount();}
 std::uint64_t presentCount()const noexcept override{return target.presentCount();}
};
semantic::EraseMaskRecord activeMask(unsigned tails){
 semantic::SweptCircleMask g;
 for(unsigned i=0;i<tails;++i){const double x=50+35*i;g.segments.push_back({{{x,35},5},{{x+4,90},5},{x,50},{x+4,75}});}
 return {ObjectId::fromUint64(800),std::move(g)};
}
FramePlan displayPlan(unsigned tails,std::uint64_t id,unsigned dpr,unsigned bg=0,unsigned copies=2,bool large=false){
 auto e=displaySource();e.record.content.update([](auto& v){std::get<semantic::CompactStrokeContent>(v).stroke.config.opacity=.55;});e.command=CompactStrokeReferenceCommand{scene::ImmutableValue<semantic::CompactStrokeContent>::alias(e.record.content)};
 e.presentationEraseMasks={activeMask(tails)};const auto base=renderPlan(e,id,dpr).frame;
 const float width=large?800:256,height=large?500:128;
 FrameState f{base.viewId,base.camera,{0,0,width,height},
  {width,height,std::uint32_t(width*dpr),std::uint32_t(height*dpr),float(dpr),1},base.sceneGeneration,base.sceneReadToken,base.surfaceGeneration,base.metricsGeneration,base.frameId,{0,0,width,height}};
 ReferenceDrawList draw{f};draw.viewportClip=f.viewportClip;
 for(unsigned i=0;i<copies;++i){auto next=e;next.record.objectId=ObjectId::fromUint64(i+1);next.record.transform.tx=i*3.;draw.entries.push_back(std::move(next));}
 foundation::DocumentBackground background;if(bg==2)background.color={.25F,.5F,.75F,1};
 return {f,std::move(draw),background,bg==1,true};
}

ReferenceTraversalEntry continuityShape(std::uint64_t id, double x, double y,
    float red, float green, float blue) {
 auto object=semantic::ObjectRecord{}; object.id=ObjectId::fromUint64(id);
 object.kind=semantic::ObjectKind::kShape; object.kind_version=1;
 object.content=semantic::ShapeContent{1,90,52}; object.transform.tx=x; object.transform.ty=y;
 object.properties.entries={{0x100U,semantic::FillStyleValue{semantic::SolidFill{{red,green,blue,1.0F}}}}};
 const auto projected=projectRuntimeScene({&object,1},semantic::SemanticGeneration{1});
 return {projected.records.front(),true,ShapeReferenceCommand{std::get<semantic::ShapeContent>(object.content)}};
}

FramePlan objectContinuityPlan(std::uint64_t id, const ReferenceTraversalEntry& before,
    const ReferenceTraversalEntry& active, const ReferenceTraversalEntry& after,
    std::uint64_t surfaceGeneration=1) {
 const auto base=displayPlan(1,id,1).frame;
 FrameState frame{base.viewId,base.camera,{0,0,256,128},
   {256,128,256,128,1,1},semantic::SemanticGeneration{1},SceneRevision{1},
   SurfaceGeneration{surfaceGeneration},base.metricsGeneration,FrameId{id},{0,0,256,128}};
 ReferenceDrawList draw{frame}; draw.viewportClip=frame.viewportClip;
 draw.entries={before,active,after};
 return {frame,std::move(draw),{},false,false};
}

std::vector<std::uint8_t> pixels(DisplayTarget&,const FramePlan&);
std::string pixelDigest(const std::vector<std::uint8_t>& bytes) {
 std::uint64_t hash=1469598103934665603ULL;
 for(const auto byte:bytes){hash^=byte;hash*=1099511628211ULL;}
 std::ostringstream value;value<<std::hex<<hash;return value.str();
}

void objectContinuityStaticReuse(const std::filesystem::path& out) {
 std::filesystem::create_directories(out);
 auto device=MTLCreateSystemDefaultDevice(),queue=message(device,"newCommandQueue");
 auto completion=std::make_shared<NativeMetalCompletion>();
 GrMtlBackendContext backend{sk_cfp<GrMTLHandle>(device),sk_cfp<GrMTLHandle>(queue)};
 completion->context=GrDirectContexts::MakeMetal(backend);
 DisplayTarget target(completion),reference(completion);
 require(target.resize(256,128).code==BackendSubmissionCode::kAccepted,"object continuity target");
 require(reference.resize(256,128).code==BackendSubmissionCode::kAccepted,"object continuity oracle");
 auto before=continuityShape(41001,8,24,.15F,.25F,.85F);
 auto active=continuityShape(41002,82,38,.85F,.20F,.15F);
 auto after=continuityShape(41003,166,60,.15F,.70F,.25F);
 SkiaRenderer cached,oracle; RenderCacheOptions disabled; disabled.enabled=false; oracle.setRenderCacheOptions(disabled);
 const auto initial=objectContinuityPlan(100, before, active, after);
 require(cached.renderFrame(target,initial).code==BackendSubmissionCode::kAccepted,"object continuity canonical warm");
 require(oracle.renderFrame(reference,initial).code==BackendSubmissionCode::kAccepted,"object continuity oracle warm");
 EditingOverlay overlay(initial.frame);
 require(overlay.update({.selectedObject=active.record.objectId,.worldBounds=active.record.worldBounds,
                         .objectTransform=active.record.transform,.selected=true,.active=true}),
         "object continuity selection chrome");
 const auto directBefore=cached.renderCacheWork().directGroups;
 for(unsigned i=0;i<3;++i){
   active.record.transform.tx+=7.0;
   const auto moved=objectContinuityPlan(101+i,before,active,after);
   require(cached.renderTransientOverlay(target,moved,&overlay).code==BackendSubmissionCode::kAccepted,
           "object continuity cached move");
   require(oracle.renderTransientOverlay(reference,moved,&overlay).code==BackendSubmissionCode::kAccepted,
           "object continuity direct move");
   const auto actual=pixels(target,moved),expected=pixels(reference,moved);
   saveBuffer(out/(std::to_string(i)+"-actual.rgba"),actual);
   saveBuffer(out/(std::to_string(i)+"-oracle.rgba"),expected);
   std::ofstream(out/(std::to_string(i)+"-pixels.json"))<<nlohmann::json{
     {"actual_digest",pixelDigest(actual)},{"oracle_digest",pixelDigest(expected)},
     {"oracle_static_builds",oracle.objectInteractionWork().builds},
     {"oracle_static_hits",oracle.objectInteractionWork().hits}}<<'\n';
   require(oracle.objectInteractionWork().builds==0&&oracle.objectInteractionWork().hits==0,
           "disabled independent oracle entered static optimization");
   require(actual==expected,"object continuity pixel/order mismatch");
 }
 const auto directDelta=cached.renderCacheWork().directGroups-directBefore;
 const auto work=cached.objectInteractionWork();
 const auto actual=pixels(target,objectContinuityPlan(104,before,active,after));
 const auto expected=pixels(reference,objectContinuityPlan(104,before,active,after));
 std::ofstream(out/"old-static-reuse-red.json")
     << nlohmann::json{{"direct_groups_delta",directDelta},{"moves",3},
       {"static_reuse_builds",work.builds},{"static_reuse_hits",work.hits},
       {"static_reuse_invalidations",work.invalidations},
       {"full_transient_scene_draws",work.fullSceneDraws},{"active_object_draws",work.activeDraws},
       {"static_reuse_lease_bytes",work.leaseBytes},
       {"actual_pixel_digest",pixelDigest(actual)},{"oracle_pixel_digest",pixelDigest(expected)}}
     << '\n';
 // The old renderer redraws all three non-ink entries on every move. The
 // production implementation must keep the unchanged prefix/suffix stable.
 require(directDelta<=5,"static scene redraw was not reduced to active object work");
}
std::vector<std::uint8_t> pixels(DisplayTarget& t,const FramePlan& p){std::vector<std::uint8_t> out(std::size_t(p.frame.metrics.physicalWidth)*p.frame.metrics.physicalHeight*4);require(t.readbackRgba(out).code==BackendSubmissionCode::kAccepted,"actual target capture");return out;}
void settle(SkiaRenderer& r,DisplayTarget& t,const FramePlan& p,const std::shared_ptr<NativeMetalCompletion>& c){
 EditingOverlay overlay{p.frame};
 for(unsigned i=0;i<2048;++i){auto result=r.renderTransientOverlay(t,p,&overlay);c->context->flushAndSubmit(GrSyncCpu::kYes);r.serviceResourceRetirements();if(result.code==BackendSubmissionCode::kAccepted)return;if(result.code!=BackendSubmissionCode::kRetry)std::cerr<<result.message<<'\n';require(result.code==BackendSubmissionCode::kRetry,"newest request rejected");}
 require(false,"stable feasible transient demand did not progress");
}
void continuity(const std::filesystem::path& out){
 std::filesystem::create_directories(out);auto device=MTLCreateSystemDefaultDevice(),queue=message(device,"newCommandQueue");auto c=std::make_shared<NativeMetalCompletion>();GrMtlBackendContext backend{sk_cfp<GrMTLHandle>(device),sk_cfp<GrMTLHandle>(queue)};c->context=GrDirectContexts::MakeMetal(backend);
 for(unsigned dpr:{1U,2U,4U})for(unsigned bg=0;bg<3;++bg){
  DisplayTarget target(c),reference(c);require(target.resize(256*dpr,128*dpr).code==BackendSubmissionCode::kAccepted&&reference.resize(256*dpr,128*dpr).code==BackendSubmissionCode::kAccepted,"continuity targets");
  SkiaRenderer r;auto a=displayPlan(1,1,dpr,bg);settle(r,target,a,c);const auto before=pixels(target,a);const auto presents=target.presentCount(),submits=r.submissionCount(),builds=r.renderCacheWork().preparedBodyBuilds;
  const auto fail=[&](const FramePlan& p,const char* stage){EditingOverlay overlay{p.frame};const auto result=r.renderTransientOverlay(target,p,&overlay);c->context->flushAndSubmit(GrSyncCpu::kYes);r.serviceResourceRetirements();auto after=pixels(target,p);
   saveBuffer(out/(std::to_string(dpr)+"-"+std::to_string(bg)+"-"+stage+"-before.rgba"),before);saveBuffer(out/(std::to_string(dpr)+"-"+std::to_string(bg)+"-"+stage+"-after.rgba"),after);
   std::cout<<nlohmann::json{{"DPR",dpr},{"background",bg},{"stage",stage},{"status",static_cast<unsigned>(result.code)},{"message",result.message},{"pixels_preserved",after==before},{"presents",target.presentCount()},{"body_builds",r.renderCacheWork().preparedBodyBuilds}}.dump()<<std::endl;
   require(result.code!=BackendSubmissionCode::kAccepted,"failure stage accepted");require(after==before,"failed attempt changed last display");require(target.presentCount()==presents&&r.submissionCount()==submits,"failed attempt counted as successful display");};
  auto b=displayPlan(2,2,dpr,bg);transientDisplayFault.beforeObject=1;fail(b,"after-one-object");
  auto newest=displayPlan(3,3,dpr,bg);transientDisplayFault.afterTile=0;fail(newest,"after-one-tile");
  target.unavailable=true;fail(displayPlan(3,4,dpr,bg),"acquire-unavailable");target.unavailable=false;
  transientDisplayFault.beforeObject=0;fail(displayPlan(3,5,dpr,bg),"before-first-object");
  transientDisplayFault.beforePublish=true;fail(displayPlan(3,6,dpr,bg),"prepublication-refusal");
  auto final=displayPlan(3,7,dpr,bg);settle(r,target,final,c);auto actual=pixels(target,final);
  SkiaRenderer full;settle(full,reference,final,c);auto expected=pixels(reference,final);saveBuffer(out/(std::to_string(dpr)+"-"+std::to_string(bg)+"-latest.rgba"),actual);saveBuffer(out/(std::to_string(dpr)+"-"+std::to_string(bg)+"-full-reference.rgba"),expected);
  require(actual==expected,"cumulative failed B/C damage lost or opacity applied twice");require(r.renderCacheWork().preparedBodyBuilds==builds,"failed display continuity caused full body rebuild");require(r.renderCacheWork().eraseDamageFrames>0,"test did not enter incremental erase");require(r.renderCacheWork().cpuCoverageTiles==0,"strict v4 used CPU coverage");
  // Independent original null-cache/full-history CPU drawing remains separate.
  DisplayTarget cpu(c);require(cpu.resize(256*dpr,128*dpr).code==BackendSubmissionCode::kAccepted,"CPU reference target");auto acq=cpu.acquire();require(drawReferencePlanToSkCanvas(*acq.frame.surface->getCanvas(),final).code==BackendSubmissionCode::kAccepted,"original full reference");cpu.release();require(cpu.present().code==BackendSubmissionCode::kAccepted,"reference present");auto original=pixels(cpu,final);std::vector<bool> edge(original.size()/4);
  for(const auto& entry:final.referenceDrawList.entries){auto e=independentEdges(entry,dpr);for(std::size_t i=0;i<edge.size();++i)edge[i]=edge[i]||e[i];}
  for(std::size_t i=0;i<original.size();++i)if(!edge[i/4])require(std::abs(int(actual[i])-int(original[i]))<=2,"latest outside original reference AA band");
  std::cout<<nlohmann::json{{"DPR",dpr},{"background",bg},{"latest_equals_independent_full_gpu",true},{"original_AA_reference",true},{"body_builds",builds},{"damage_frames",r.renderCacheWork().eraseDamageFrames}}.dump()<<std::endl;
 }
}
void recovery(const std::filesystem::path& out){
 std::filesystem::create_directories(out);auto device=MTLCreateSystemDefaultDevice(),queue=message(device,"newCommandQueue");auto c=std::make_shared<NativeMetalCompletion>();GrMtlBackendContext backend{sk_cfp<GrMTLHandle>(device),sk_cfp<GrMTLHandle>(queue)};c->context=GrDirectContexts::MakeMetal(backend);DisplayTarget target(c);require(target.resize(3200,2000).code==BackendSubmissionCode::kAccepted,"DPR4 pressure target");SkiaRenderer r;auto a=displayPlan(1,1,4,0,16,true);settle(r,target,a,c);auto before=pixels(target,a);auto budget=transientDisplayBudget.lock();require(bool(budget),"test-only existing budget witness");
 const auto available=96U*1024U*1024U-budget->used();const auto token=budget->reserve(resources::ResourceClass::kRecorded,available-1024,true);require(bool(token),"admit held competing quota");resources::ResourceLease hold(budget,*token);const auto presents=target.presentCount();
 for(unsigned i=0;i<3;++i){auto p=displayPlan(i+2,i+2,4,0,16,true);EditingOverlay overlay{p.frame};auto result=r.renderTransientOverlay(target,p,&overlay);c->context->flushAndSubmit(GrSyncCpu::kYes);r.serviceResourceRetirements();auto after=pixels(target,p);require(result.code==BackendSubmissionCode::kRetry&&after==before&&target.presentCount()==presents,"genuine quota Retry changed display");std::cout<<nlohmann::json{{"pressure_attempt",i},{"used_bytes",budget->used()},{"hard_bytes",96U*1024U*1024U},{"pixels_preserved",true},{"GPU_epochs",budget->pendingGpuEpochCount()}}.dump()<<std::endl;}
 hold.reset();c->context->flushAndSubmit(GrSyncCpu::kYes);r.serviceResourceRetirements();auto newest=displayPlan(4,9,4,0,16,true);settle(r,target,newest,c);require(target.presentCount()==presents+1,"quota release did not publish newest exactly once");require(budget->used()<=96U*1024U*1024U,"new target escaped original hard cap");
 const auto count=target.presentCount(),submits=r.submissionCount();target.refusePresent=true;EditingOverlay overlay{newest.frame};require(r.renderTransientOverlay(target,newest,&overlay).code!=BackendSubmissionCode::kAccepted,"provider rejection accepted");require(target.presentCount()==count&&r.submissionCount()==submits,"provider rejection fabricated receipt");target.refusePresent=false;settle(r,target,displayPlan(4,10,4,0,16,true),c);
 require(target.advanceGeneration().code==BackendSubmissionCode::kAccepted,"generation advance");require(r.renderTransientOverlay(target,newest,&overlay).code==BackendSubmissionCode::kRejected,"stale target presented");
 r.clearRenderCache();c->context->flushAndSubmit(GrSyncCpu::kYes);r.serviceResourceRetirements();require(budget->used()==0,"derived leases survived clear plus real GPU completion");std::cout<<nlohmann::json{{"DPR4_default96_progress",true},{"retries",3},{"after_clear_bytes",budget->used()},{"no_fake_present",true}}.dump()<<std::endl;
 // The accepted P36 NotAdmitted path must also progress behind the new
 // transactional display target, not only through the lower-level helper.
 DisplayTarget historyTarget(c);require(historyTarget.resize(1024,512).code==BackendSubmissionCode::kAccepted,"history display target");auto start=displayPlan(1,11,4,0,1);settle(r,historyTarget,start,c);auto stable=pixels(historyTarget,start);r.clearRenderCache();c->context->flushAndSubmit(GrSyncCpu::kYes);r.serviceResourceRetirements();require(budget->used()==0,"history pressure fixture must first retire evictable work");
 auto draw=start.referenceDrawList;semantic::SweptCircleMask largeHistory;largeHistory.segments.assign(40000,semantic::EraseCubicSegment{{{80,40},2},{{80,80},2},{80,50},{80,70}});draw.entries[0].record.eraseMasks={{ObjectId::fromUint64(950),std::move(largeHistory)}};FramePlan large{start.frame,std::move(draw),{},false,true};EditingOverlay largeOverlay{large.frame};
 const auto held=budget->reserve(resources::ResourceClass::kRecorded,96U*1024U*1024U-budget->used()-1024,true);require(bool(held),"large history quota hold");resources::ResourceLease historyPressure(budget,*held);
 for(unsigned i=0;i<2;++i){require(r.renderTransientOverlay(historyTarget,large,&largeOverlay).code==BackendSubmissionCode::kRetry,"history pressure did not retry");c->context->flushAndSubmit(GrSyncCpu::kYes);r.serviceResourceRetirements();require(pixels(historyTarget,large)==stable,"history pressure destroyed last display");}
 historyPressure.reset();c->context->flushAndSubmit(GrSyncCpu::kYes);r.serviceResourceRetirements();settle(r,historyTarget,large,c);require(r.renderCacheWork().maskHistory.denied>0&&r.renderCacheWork().cpuCoverageTiles==0,"history nonadmission bypassed GPU recovery");
 std::cout<<nlohmann::json{{"actual_renderer_40000_history_progress",true},{"history_denials",r.renderCacheWork().maskHistory.denied},{"CPU_tiles",r.renderCacheWork().cpuCoverageTiles},{"used_bytes",budget->used()}}.dump()<<std::endl;
 r.clearRenderCache();c->context->flushAndSubmit(GrSyncCpu::kYes);r.serviceResourceRetirements();require(budget->used()==0,"history display leases did not retire");

}


FramePlan handoffPlan(unsigned copies,std::uint64_t id,std::uint64_t generation=1){auto p=workingSetPlan(copies,true,4);const auto& b=p.frame;
 FrameState f{b.viewId,b.camera,{0,0,1024,576},{1024,576,4096,2304,4,1},semantic::SemanticGeneration{id},SceneRevision{id},SurfaceGeneration{generation},b.metricsGeneration,FrameId{id},{0,0,1024,576}};
 ReferenceDrawList d{f};d.viewportClip=f.viewportClip;d.entries=p.referenceDrawList.entries;for(auto& e:d.entries)e.presentationEraseActive=true;return{f,std::move(d),{},false,true};}
bool handoffProgress(const std::filesystem::path& out){
 std::filesystem::create_directories(out);auto device=MTLCreateSystemDefaultDevice(),queue=message(device,"newCommandQueue");auto c=std::make_shared<NativeMetalCompletion>();GrMtlBackendContext backend{sk_cfp<GrMTLHandle>(device),sk_cfp<GrMTLHandle>(queue)};c->context=GrDirectContexts::MakeMetal(backend);bool all=true;
 for(bool warm:{false,true}){DisplayTarget target(c);require(target.resize(4096,2304).code==BackendSubmissionCode::kAccepted,"handoff target");auto acq=target.acquire();acq.frame.surface->getCanvas()->clear(SK_ColorWHITE);target.release();require(target.present().code==BackendSubmissionCode::kAccepted,"initial valid provider display");SkiaRenderer r;
  if(warm){const auto a=handoffPlan(1,1);settle(r,target,a,c);}
  const auto next=handoffPlan(513,2);EditingOverlay o{next.frame};const auto before=pixels(target,next);const auto priorPresents=target.presentCount();saveBuffer(out/(warm?"warm-before.rgba":"fresh-before.rgba"),before);bool accepted=false;unsigned calls=0;
  for(;calls<1024;++calls){auto result=r.renderTransientOverlay(target,next,&o);c->context->flushAndSubmit(GrSyncCpu::kYes);r.serviceResourceRetirements();const auto actual=pixels(target,next);const auto b=transientDisplayBudget.lock();const auto w=r.renderCacheWork();
   std::cout<<nlohmann::json{{"warm",warm},{"attempt",calls},{"status",unsigned(result.code)},{"message",result.message},{"used",b->used()},{"canonical_frame_bytes",b->used(resources::ResourceClass::kCanonicalFrame)},{"scratch_bytes",b->used(resources::ResourceClass::kScratch)},{"GPU_epochs",b->pendingGpuEpochCount()},{"body_builds",w.preparedBodyBuilds},{"presents",target.presentCount()}}.dump()<<std::endl;
   require(b->used()<=96U*1024U*1024U,"handoff exceeded original hard cap");if(result.code==BackendSubmissionCode::kAccepted){accepted=true;saveBuffer(out/(warm?"warm-accepted.rgba":"fresh-accepted.rgba"),actual);break;}
   require(result.code==BackendSubmissionCode::kRetry,"handoff unexpectedly rejected");if(actual!=before)saveBuffer(out/(warm?"warm-failed.rgba":"fresh-failed.rgba"),actual);require(actual==before&&target.presentCount()==priorPresents,"handoff Retry changed actual last display");
  }
  std::cout<<nlohmann::json{{"handoff_summary",true},{"warm",warm},{"accepted",accepted},{"calls",calls+unsigned(accepted)},{"CPU_tiles",r.renderCacheWork().cpuCoverageTiles}}.dump()<<std::endl;
  all&=accepted;require(r.renderCacheWork().preparedBodyBuilds<=2,"handoff Retry rebuilt identical body");require(r.renderCacheWork().cpuCoverageTiles==0,"handoff CPU fallback");r.clearRenderCache();c->context->flushAndSubmit(GrSyncCpu::kYes);r.serviceResourceRetirements();
 }
 if(!all)std::cerr<<"G5_HO01_FEASIBLE_WARM_STAGED_DEMAND_DID_NOT_ACCEPT\n";return all;
}


void handoffInflight(const std::filesystem::path& out){
 std::filesystem::create_directories(out);auto device=MTLCreateSystemDefaultDevice(),queue=message(device,"newCommandQueue");auto c=std::make_shared<NativeMetalCompletion>();GrMtlBackendContext backend{sk_cfp<GrMTLHandle>(device),sk_cfp<GrMTLHandle>(queue)};c->context=GrDirectContexts::MakeMetal(backend);DisplayTarget target(c);require(target.resize(4096,2304).code==BackendSubmissionCode::kAccepted,"inflight target");SkiaRenderer r;auto a=handoffPlan(1,1);settle(r,target,a,c);auto before=pixels(target,a);auto budget=transientDisplayBudget.lock();auto attempt=transientDisplayAttempt.lock();require(bool(attempt)&&bool(budget),"private charged attempt observer");
 const auto epoch=budget->beginGpuEpoch(resources::kPixelResourceClasses,resources::resourceClassMask(resources::ResourceClass::kCanonicalFrame));require(bool(epoch),"delayed real GPU read epoch");require(attempt->pixels.markGpuUse(),"pin actual attempt read");
 const auto token=budget->reserve(resources::ResourceClass::kScratch,16*16*4,true);require(bool(token),"sample target quota");resources::ResourceLease sampleLease(budget,*token);auto sample=attempt->surface->getCanvas()->makeSurface(SkImageInfo::MakeN32Premul(16,16));require(bool(sample),"GPU sample target");sample->getCanvas()->drawImage(attempt->surface->makeImageSnapshot(),0,0);require(budget->endGpuEpoch(*epoch),"end actual read epoch");
 struct Gate{bool physicalDone=false;} gate;GrFlushInfo info;info.fFinishedContext=&gate;info.fFinishedProc=[](void* p){static_cast<Gate*>(p)->physicalDone=true;};c->context->flush(info);require(c->context->submit(GrSyncCpu::kNo),"actual delayed sample submit");sample.reset();sampleLease.reset();attempt.reset();
 auto large=handoffPlan(513,2);EditingOverlay overlay{large.frame};const auto presents=target.presentCount();auto result=r.renderTransientOverlay(target,large,&overlay);c->context->flushAndSubmit(GrSyncCpu::kYes);c->service();r.serviceResourceRetirements();
 require(result.code==BackendSubmissionCode::kRetry,"unacknowledged old GPU read must keep quota pending");require(gate.physicalDone,"physical GPU completion not observed");require(transientDisplayAttempt.expired(),"staged path still owns obsolete attempt");require(budget->snapshot(resources::ResourceClass::kCanonicalFrame).pendingRelease>=36U*1024U*1024U,"dropping owner freed inflight quota early");require(pixels(target,large)==before&&target.presentCount()==presents,"inflight handoff damaged last provider display");
 std::cout<<nlohmann::json{{"stage","delayed-completion"},{"GPU_epoch",*epoch},{"physical_finished",gate.physicalDone},{"pending_release",budget->snapshot(resources::ResourceClass::kCanonicalFrame).pendingRelease},{"used",budget->used()},{"actual_pixels_preserved",true}}.dump()<<std::endl;
 require(budget->completeGpuEpoch(*epoch),"ack real finished read epoch");r.serviceResourceRetirements();settle(r,target,large,c);require(target.presentCount()==presents+1,"latest staged frame did not publish once");
 for(unsigned cycle=0;cycle<2;++cycle){auto small=handoffPlan(1,3+cycle*2);settle(r,target,small,c);require(!transientDisplayAttempt.expired(),"smaller demand never returned to normal residency");auto next=handoffPlan(513,4+cycle*2);settle(r,target,next,c);require(transientDisplayAttempt.expired(),"repeat staged switch retained unused third plane");}
 require(budget->used()<=96U*1024U*1024U&&r.renderCacheWork().cpuCoverageTiles==0,"switch escaped original GPU/budget policy");
 require(target.resize(4096,2304).code==BackendSubmissionCode::kAccepted,"logical rebind");const auto oldCount=target.presentCount();require(r.renderTransientOverlay(target,large,&overlay).code==BackendSubmissionCode::kRejected&&target.presentCount()==oldCount,"stale generation presented");auto rebound=handoffPlan(1,8,target.generation());settle(r,target,rebound,c);r.clearRenderCache();c->context->flushAndSubmit(GrSyncCpu::kYes);r.serviceResourceRetirements();require(budget->used()==0,"completed switch resources did not converge to zero");
 std::cout<<nlohmann::json{{"stage","switches-complete"},{"cycles",2},{"generation",target.generation()},{"used",budget->used()},{"CPU_tiles",r.renderCacheWork().cpuCoverageTiles}}.dump()<<std::endl;
}

#endif
}
int main(int argc,char** argv){try{
#if defined(__APPLE__)
 auto pool=message(message(reinterpret_cast<id>(objc_getClass("NSAutoreleasePool")),"alloc"),"init");{const std::string mode=argc>1?argv[1]:"--pixels";const auto out=std::filesystem::path(argc>2?argv[2]:"out/transient-display-pixels");if(mode=="--handoff-inflight")handoffInflight(out);else if(mode=="--handoff"){if(!handoffProgress(out)){message(pool,"drain");return 1;}}else if(mode=="--continuity")continuity(out);else if(mode=="--object-continuity")objectContinuityStaticReuse(out);else if(mode=="--recovery")recovery(out);else failedPixels(mode=="--pixels"?out:std::filesystem::path(mode));}message(pool,"drain");
#else
 (void)argc;(void)argv;
#endif
 return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
