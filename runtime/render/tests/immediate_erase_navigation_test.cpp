// Actual renderer/provider regression. No timing assertion or mock preparation.
#define main ienPriorPreparedMain
#include "canonical_prepared_body_test.cpp"
#undef main
#include "../../interaction/src/partial_erase_geometry.hpp"
#include "canvas/semantic/snapshot.hpp"
#include "canvas/render/direct_reference_source.hpp"
#include <nlohmann/json.hpp>
#include <iostream>
#if defined(__APPLE__)
FramePlan ienPlan(const semantic::SemanticSnapshot& snapshot,const RuntimeSceneProjection& projected,std::uint64_t id,double dx,bool committed=false){
 const float z=.34F,tx=120+dx,ty=260-dx*.5;
 FrameState f{ViewId{1},CameraState{{(715.5F-tx)/z,(505.F-ty)/z},z,0,CameraGeneration{id}},
   {-tx/z,-ty/z,(1431-tx)/z,(1010-ty)/z},{1431,1010,2862,2020,2,1},semantic::SemanticGeneration{committed?1U:0U},SceneRevision{committed?2U:1U},SurfaceGeneration{1},MetricsGeneration{1},FrameId{id},{0,0,1431,1010}};
 ReferenceDrawList d{f};d.viewportClip=f.viewportClip;d.worldToView={z,0,0,z,tx,ty};
 std::vector<foundation::WorldPoint> trace;for(unsigned i=0;i<=480;++i)trace.push_back({float(420+80*std::sin(2*3.141592653589793*i/160)),float(160+540.*i/480)});
 for(const auto& record:projected.records){auto command=DirectReferenceSource::presentationCommand(record);require(bool(command),"actual reference command");ReferenceTraversalEntry e{record,true,std::move(command.value())};
  if(record.kind==semantic::ObjectKind::kVectorStroke||record.kind==semantic::ObjectKind::kDabStroke){auto mask=interaction::partialEraseGeometry(trace,record.transform,16);require(bool(mask),"real mask");if(committed)e.record.eraseMasks.push_back({ObjectId::fromUint64(900000+d.entries.size()),std::move(*mask)});else{e.presentationEraseActive=true;e.presentationEraseMasks.push_back({{},std::move(*mask)});}}
  d.entries.push_back(std::move(e));
 }
 return{f,std::move(d),snapshot.background,false,true};
}
#endif
int main(int argc,char** argv){try{
#if defined(__APPLE__)
 require(argc>=3,"fixture output");std::ifstream file(argv[1],std::ios::binary);std::vector<std::uint8_t> bytes{std::istreambuf_iterator<char>(file),{}};auto decoded=semantic::SnapshotCodec::decode(bytes,semantic::SnapshotCapability::kAllWithCompactStrokeV1);require(decoded.ok(),"fixture decode");
 auto pool=message(message(reinterpret_cast<id>(objc_getClass("NSAutoreleasePool")),"alloc"),"init");auto device=MTLCreateSystemDefaultDevice(),queue=message(device,"newCommandQueue");auto c=std::make_shared<NativeMetalCompletion>();GrMtlBackendContext backend{sk_cfp<GrMTLHandle>(device),sk_cfp<GrMTLHandle>(queue)};c->context=GrDirectContexts::MakeMetal(backend);require(bool(c->context),"Metal");
 NativeMetalTarget target(c);require(target.resize(2862,2020).code==BackendSubmissionCode::kAccepted,"target");SkiaRenderer renderer;RenderCacheOptions options;options.maxBytes=512U*1048576;options.sharedMaxBytes=1024U*1048576;options.maxEntryBytes=128U*1048576;renderer.setRenderCacheOptions(options);
 auto identityOwner=std::make_shared<int>(7);TransientDisplayIdentity identity{identityOwner,3,481};
 const auto projected=projectRuntimeScene(decoded.snapshot->objects,semantic::SemanticGeneration{0});
 auto initial=ienPlan(*decoded.snapshot,projected,1,0);EditingOverlay overlay{initial.frame};bool ready=false;
 for(unsigned i=0;i<1024;++i){auto r=renderer.renderTransientOverlay(target,initial,&overlay,identity);c->context->flushAndSubmit(GrSyncCpu::kYes);renderer.serviceResourceRetirements();require(r.code!=BackendSubmissionCode::kRejected,r.message.c_str());if(r.code==BackendSubmissionCode::kAccepted){ready=true;break;}}
 require(ready,"initial complete pending display");
 auto firstAcquire=target.acquire();auto completeInitial=firstAcquire.frame.surface->makeImageSnapshot();target.release();require(bool(completeInitial),"complete initial snapshot");
 unsigned submitted=0;std::string reason;
 for(unsigned i=0;i<8;++i){auto p=ienPlan(*decoded.snapshot,projected,2+i,1+i);EditingOverlay o{p.frame};const auto count=target.presentCount();const auto r=renderer.renderTransientOverlay(target,p,&o,identity);c->context->flushAndSubmit(GrSyncCpu::kYes);renderer.serviceResourceRetirements();if(r.code==BackendSubmissionCode::kAccepted&&target.presentCount()>count)++submitted;else reason=r.message;}
 std::ofstream(argv[2])<<nlohmann::json{{"current_camera_submissions",submitted},{"requested_camera_changes",8},{"last_reason",reason},{"CPU_tiles",renderer.renderCacheWork().cpuCoverageTiles}}.dump(2)<<'\n';
 require(submitted==8,"PENDING_CAMERA_MATERIAL_RESTART_STARVES_COMPLETE_FEEDBACK");
 // Effective committed masks must be proven from full values and body/pose.
 auto committed=ienPlan(*decoded.snapshot,projected,20,12,true);
 const NavigationBackgroundQuery empty=[&](const foundation::WorldRect& region){return std::none_of(committed.referenceDrawList.entries.begin(),committed.referenceDrawList.entries.end(),[&](const auto& e){return e.contributesPixels&&e.record.worldBounds.intersects(region);});};
 auto navigation=renderer.renderNavigation(target,{.34F,132,254},committed.frame,{},false,nullptr,empty,&committed);
 require(navigation.code==BackendSubmissionCode::kAccepted&&renderer.navigationWork().presented,"COMMITTED_EQUIVALENT_MASK_LOST_FEEDBACK");
 // Independent full-frame camera witness, no renderer/cache/helper reuse.
 std::vector<std::uint8_t> actual(2862U*2020U*4U),expected(actual.size());require(target.readbackRgba(actual).code==BackendSubmissionCode::kAccepted,"handoff readback");
 auto oracle=SkSurfaces::RenderTarget(c->context.get(),skgpu::Budgeted::kNo,SkImageInfo::MakeN32Premul(2862,2020));require(bool(oracle),"camera oracle surface");auto* oc=oracle->getCanvas();oc->clear(SK_ColorWHITE);oc->translate(24,-12);oc->drawImage(completeInitial,0,0,SkSamplingOptions(SkFilterMode::kLinear));c->context->flushAndSubmit(GrSyncCpu::kYes);
 require(oracle->readPixels(SkImageInfo::Make(2862,2020,kRGBA_8888_SkColorType,kPremul_SkAlphaType),expected.data(),2862*4,0,0)&&expected==actual,"FULL_CURRENT_CAMERA_FRAME_DIFFERS_INDEPENDENT_RESAMPLE");
 // Erase interior is world-space and stays clear after fractional camera move.
 unsigned clearInterior=0;for(unsigned y=240;y<650;y+=40){const int px=int((420*.34+132)*2),py=int((y*.34+254)*2);const auto at=(py*2862+px)*4;if(actual[at]>240&&actual[at+1]>240&&actual[at+2]>240)++clearInterior;}
 require(clearInterior>0,"erased content resurrected");
 // Complete pixels may never cross a stale surface/metrics/provider or an
 // altered canonical body/mask. These use actual GPU targets, not counters.
 const auto count=target.presentCount();
 auto wrongMaskDraw=committed.referenceDrawList;for(auto& entry:wrongMaskDraw.entries)if(!entry.record.eraseMasks.empty()){auto& g=std::get<semantic::SweptCircleMask>(entry.record.eraseMasks.back().geometry);g.segments.back().p1.radius+=1;break;}
 FramePlan wrongMask{committed.frame,std::move(wrongMaskDraw),committed.background,false,true};
 auto wrongMaskResult=renderer.renderNavigation(target,{.34F,132,254},wrongMask.frame,{},false,nullptr,empty,&wrongMask);require(target.presentCount()==count,"changed mask reused pending plane");(void)wrongMaskResult;
 NativeMetalTarget otherProvider(c);require(otherProvider.resize(2862,2020).code==BackendSubmissionCode::kAccepted,"other target");auto otherResult=renderer.renderNavigation(otherProvider,{.34F,132,254},committed.frame,{},false,nullptr,empty,&committed);require(otherProvider.presentCount()==0,"foreign provider reused pending plane");(void)otherResult;
 FrameState staleFrame{committed.frame.viewId,committed.frame.camera,committed.frame.worldViewport,committed.frame.metrics,committed.frame.sceneGeneration,committed.frame.sceneReadToken,SurfaceGeneration{2},committed.frame.metricsGeneration,FrameId{23},committed.frame.viewportClip};
 auto staleSources=ReferenceDrawList{staleFrame};staleSources.entries=committed.referenceDrawList.entries;staleSources.worldToView=committed.referenceDrawList.worldToView;staleSources.viewportClip=committed.referenceDrawList.viewportClip;FramePlan stale{staleFrame,std::move(staleSources),committed.background,false,true};auto staleResult=renderer.renderNavigation(target,{.34F,132,254},stale.frame,{},false,nullptr,empty,&stale);require(staleResult.code!=BackendSubmissionCode::kAccepted&&target.presentCount()==count,"stale surface reused pending plane");
 // One actual no-hit decision proves that a presentation mask can disappear
 // at semantic commit for only that unchanged object, not by mask-count guess.
 auto missSource=source();missSource.record.objectId=ObjectId::fromUint64(920001);
 auto missContent=std::get<CompactStrokeReferenceCommand>(missSource.command).content.get();missContent.stroke.knots={{2000,600,32768},{2100,600,32768}};missSource.record.content=missContent;missSource.command=CompactStrokeReferenceCommand{scene::ImmutableValue<semantic::CompactStrokeContent>::alias(missSource.record.content)};
 missSource.record.kind=semantic::ObjectKind::kVectorStroke;missSource.record.kindVersion=4;missSource.record.worldBounds={1980,580,2120,620};missSource.record.geometryBounds=missSource.record.worldBounds;missSource.record.visualBounds=missSource.record.worldBounds;missSource.presentationEraseActive=true;
 std::vector<foundation::WorldPoint> missTrace{{420,160},{420,700}};auto missMask=interaction::partialEraseGeometry(missTrace,{},16);require(bool(missMask),"miss mask");missSource.presentationEraseMasks.push_back({{},*missMask});
 auto withMissDraw=initial.referenceDrawList;withMissDraw.entries.push_back(missSource);FramePlan withMiss{initial.frame,std::move(withMissDraw),initial.background,false,true};
 SkiaRenderer nohitRenderer;nohitRenderer.setRenderCacheOptions(options);bool missReady=false;
 for(unsigned k=0;k<1024;++k){auto r=nohitRenderer.renderTransientOverlay(target,withMiss,&overlay,identity);c->context->flushAndSubmit(GrSyncCpu::kYes);nohitRenderer.serviceResourceRetirements();if(r.code==BackendSubmissionCode::kAccepted){missReady=true;break;}require(r.code!=BackendSubmissionCode::kRejected,r.message.c_str());}require(missReady,"nohit complete initial");
 auto decision=nohitRenderer.requestCpuLocalExact(missSource.record,*missMask);require(bool(decision),"actual nohit request");bool missProven=false;for(unsigned k=0;k<256;++k){auto status=nohitRenderer.advanceCpuLocalExact(decision);if(status==LocalExactStatus::kMiss){missProven=true;break;}require(status==LocalExactStatus::kPending,"expected disjoint exact decision");}require(missProven,"actual exact miss");
 auto nohitCommitDraw=committed.referenceDrawList;auto nohitRecord=missSource;nohitRecord.presentationEraseActive=false;nohitRecord.presentationEraseMasks.clear();nohitCommitDraw.entries.push_back(nohitRecord);FramePlan nohitCommit{committed.frame,std::move(nohitCommitDraw),committed.background,false,true};
 const auto missBefore=target.presentCount();const auto nohitNav=nohitRenderer.renderNavigation(target,{.34F,132,254},nohitCommit.frame,{},false,nullptr,empty,&nohitCommit);require(nohitNav.code==BackendSubmissionCode::kAccepted&&target.presentCount()==missBefore+1,"EXACT_NOHIT_COMMIT_LOST_COMPLETE_FEEDBACK");
 // A new complete suffix may finish during this call. Qualify its actual
 // pixels against a fresh renderer on the stable grid, then independently
 // resample that complete image to the requested camera. An old-prefix image
 // must differ, so success cannot be obtained just by dropping a retry check.
 auto nextBase=ienPlan(*decoded.snapshot,projected,21,25);auto nextDraw=nextBase.referenceDrawList;
 for(auto& entry:nextDraw.entries)if(!entry.presentationEraseMasks.empty()){
  auto& g=std::get<semantic::SweptCircleMask>(entry.presentationEraseMasks[0].geometry);
  auto tail=g.segments.back();tail.p0=tail.p1;tail.p1.position={1000,600};tail.p1.radius=2000;
  tail.control1=tail.p0.position;tail.control2=tail.p1.position;g.segments.push_back(tail);
 }
 FramePlan next{nextBase.frame,std::move(nextDraw),nextBase.background,false,true};
 const auto freshPixels=[&](const FramePlan& p){
  NativeMetalTarget independent(c);require(independent.resize(2862,2020).code==BackendSubmissionCode::kAccepted,"fresh reference target");
  SkiaRenderer fresh;fresh.setRenderCacheOptions(options);EditingOverlay bare{p.frame};bool done=false;
  for(unsigned k=0;k<1024;++k){const auto r=fresh.renderTransientOverlay(independent,p,&bare);require(r.code!=BackendSubmissionCode::kRejected,r.message.c_str());c->context->flushAndSubmit(GrSyncCpu::kYes);fresh.serviceResourceRetirements();if(r.code==BackendSubmissionCode::kAccepted){done=true;break;}}
  require(done,"independent complete suffix");auto a=independent.acquire();auto image=a.frame.surface->makeImageSnapshot();independent.release();
  auto shifted=SkSurfaces::RenderTarget(c->context.get(),skgpu::Budgeted::kNo,SkImageInfo::MakeN32Premul(2862,2020));auto* into=shifted->getCanvas();into->clear(SK_ColorWHITE);into->translate(50,-25);into->drawImage(image,0,0,SkSamplingOptions(SkFilterMode::kLinear));
  std::vector<std::uint8_t> pixels(actual.size());require(shifted->readPixels(SkImageInfo::Make(2862,2020,kRGBA_8888_SkColorType,kPremul_SkAlphaType),pixels.data(),2862*4,0,0),"independent suffix read");return pixels;
 };
 ReferenceDrawList oldGrid{initial.frame};oldGrid.entries=next.referenceDrawList.entries;oldGrid.worldToView=initial.referenceDrawList.worldToView;oldGrid.viewportClip=initial.referenceDrawList.viewportClip;
 FramePlan completeNext{initial.frame,std::move(oldGrid),next.background,false,true};const auto expectedNext=freshPixels(completeNext);
 auto oldShift=SkSurfaces::RenderTarget(c->context.get(),skgpu::Budgeted::kNo,SkImageInfo::MakeN32Premul(2862,2020));oldShift->getCanvas()->clear(SK_ColorWHITE);oldShift->getCanvas()->translate(50,-25);oldShift->getCanvas()->drawImage(completeInitial,0,0,SkSamplingOptions(SkFilterMode::kLinear));std::vector<std::uint8_t> oldPixels(actual.size());require(oldShift->readPixels(SkImageInfo::Make(2862,2020,kRGBA_8888_SkColorType,kPremul_SkAlphaType),oldPixels.data(),2862*4,0,0),"prefix reference read");
 std::size_t suffixWitness=0;for(std::size_t k=0;k<actual.size();k+=4)if(oldPixels[k]+oldPixels[k+1]+oldPixels[k+2]+24<expectedNext[k]+expectedNext[k+1]+expectedNext[k+2])++suffixWitness;
 require(suffixWitness>100,"latest suffix witness vacuous");
 const auto before=target.presentCount();EditingOverlay nextOverlay{next.frame};auto newer=identity;++newer.inputRevision;
 const auto prepared=renderer.renderTransientOverlay(target,next,&nextOverlay,newer);require(prepared.code!=BackendSubmissionCode::kRejected,"new suffix rejected");
 if(prepared.code==BackendSubmissionCode::kAccepted){require(target.presentCount()==before+1,"suffix presentation count");require(target.readbackRgba(actual).code==BackendSubmissionCode::kAccepted,"suffix actual read");require(actual==expectedNext,"LATEST_SUFFIX_CURRENT_CAMERA_PIXEL_MISMATCH");}
 else require(target.presentCount()==before,"pending suffix wrote provider");
 // Even an unchanged caller identity cannot authorize different mask values.
 // The full comparison must either prepare those new values or leave the
 // complete provider unchanged; the fresh oracle above qualifies accepted.
 auto qualified=renderer.renderTransientOverlay(target,next,&nextOverlay,newer);for(unsigned k=0;k<128&&qualified.code==BackendSubmissionCode::kRetry;++k){c->context->flushAndSubmit(GrSyncCpu::kYes);renderer.serviceResourceRetirements();qualified=renderer.renderTransientOverlay(target,next,&nextOverlay,newer);}
 require(qualified.code==BackendSubmissionCode::kAccepted,"complete suffix qualification");
 if(qualified.code==BackendSubmissionCode::kAccepted){require(target.readbackRgba(actual).code==BackendSubmissionCode::kAccepted,"qualified actual read");require(actual==expectedNext,"OLD_PREFIX_QUALIFIED_NEW_SUFFIX");}
 std::ofstream(argv[2])<<nlohmann::json{{"current_camera_submissions",submitted},{"requested_camera_changes",8},{"suffix_witness_pixels",suffixWitness},{"latest_suffix_current_camera_equal",actual==expectedNext},{"CPU_tiles",renderer.renderCacheWork().cpuCoverageTiles}}.dump(2)<<'\n';
 // A new camera exposing real content cannot loop forever preparing the old
 // grid. Actual nonempty Scene bounds require complete current-grid drawing.
 auto exposedProjection=projected;auto edge=exposedProjection.records.front();edge.objectId=ObjectId::fromUint64(940001);edge.transform.tx+=4100;edge.worldBounds.left+=4100;edge.worldBounds.right+=4100;exposedProjection.records.push_back(edge);
 auto exposureInitial=ienPlan(*decoded.snapshot,exposedProjection,48,0);EditingOverlay exposureInitialOverlay{exposureInitial.frame};SkiaRenderer exposureRenderer;exposureRenderer.setRenderCacheOptions(options);
 bool exposureSeeded=false;for(unsigned n=0;n<128;++n){auto r=exposureRenderer.renderTransientOverlay(target,exposureInitial,&exposureInitialOverlay,identity);c->context->flushAndSubmit(GrSyncCpu::kYes);exposureRenderer.serviceResourceRetirements();if(r.code==BackendSubmissionCode::kAccepted){exposureSeeded=true;break;}}require(exposureSeeded,"exposure seeded");
 auto exposedBase=ienPlan(*decoded.snapshot,exposedProjection,50,-1000);EditingOverlay exposedOverlay{exposedBase.frame};
 unsigned exposureAttempts=0;bool exposureReady=false;
 for(;exposureAttempts<128;++exposureAttempts){auto r=exposureRenderer.renderTransientOverlay(target,exposedBase,&exposedOverlay,identity);require(r.code!=BackendSubmissionCode::kRejected,"exposure rejected");c->context->flushAndSubmit(GrSyncCpu::kYes);exposureRenderer.serviceResourceRetirements();if(r.code==BackendSubmissionCode::kAccepted){exposureReady=true;break;}}
 require(exposureReady,"NONEMPTY_EXPOSURE_STUCK_PREPARING_OLD_GRID");
 require(renderer.renderCacheWork().cpuCoverageTiles==0,"CPU fallback");renderer.clearRenderCache();c->context->flushAndSubmit(GrSyncCpu::kYes);renderer.serviceResourceRetirements();message(pool,"drain");
#else
 (void)argc;(void)argv;
#endif
 return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
