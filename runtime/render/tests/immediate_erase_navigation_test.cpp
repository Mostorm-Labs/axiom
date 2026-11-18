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
 // A newer input suffix must never reuse an old complete-prefix plane.
 auto nextBase=ienPlan(*decoded.snapshot,projected,21,25);auto nextDraw=nextBase.referenceDrawList;for(auto& entry:nextDraw.entries)if(!entry.presentationEraseMasks.empty()){auto& g=std::get<semantic::SweptCircleMask>(entry.presentationEraseMasks[0].geometry);g.segments.back().p1.position.x+=17;}FramePlan next{nextBase.frame,std::move(nextDraw),nextBase.background,false,true};
 const auto before=target.presentCount();EditingOverlay nextOverlay{next.frame};auto newer=identity;++newer.inputRevision;
 auto rejected=renderer.renderTransientOverlay(target,next,&nextOverlay,newer);
 require(rejected.code==BackendSubmissionCode::kRetry&&target.presentCount()==before,"OLD_PREFIX_QUALIFIED_NEW_SUFFIX");
 // Equal serial/count, different values are also invalid even with a caller
 // identity bug. The real owner never changes a frozen trace in place.
 auto bad=renderer.renderTransientOverlay(target,next,&nextOverlay,identity);require(bad.code==BackendSubmissionCode::kRetry&&target.presentCount()==before,"MASK_VALUE_MISMATCH_REUSED_PLANE");
 require(renderer.renderCacheWork().cpuCoverageTiles==0,"CPU fallback");renderer.clearRenderCache();c->context->flushAndSubmit(GrSyncCpu::kYes);renderer.serviceResourceRetirements();message(pool,"drain");
#else
 (void)argc;(void)argv;
#endif
 return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
