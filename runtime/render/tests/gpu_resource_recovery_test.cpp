#include "../src/skia_prepared_geometry.hpp"
#include "../src/skia_render_cache_internal.hpp"
#include "../src/skia_geometry_internal.hpp"
#include "../src/compact_contour_skia_path.hpp"
#include "include/core/SkPathBuilder.h"
#include "include/core/SkPaint.h"
#include "canvas/render/skia_renderer.hpp"
#include "canvas/scene/scene_types.hpp"
#include "canvas/render/skia_scene_renderer.hpp"
#include "include/core/SkSurface.h"
#include "include/core/SkCanvas.h"
#include <tuple>
#include <fstream>
#include "canvas/ink/compact_geometry.hpp"
#include "include/core/SkMatrix.h"
#include <chrono>
#if defined(__APPLE__)
#include "include/gpu/ganesh/mtl/GrMtlBackendContext.h"
#include "include/gpu/ganesh/mtl/GrMtlDirectContext.h"
#include "include/gpu/ganesh/GrDirectContext.h"
#include "include/gpu/ganesh/SkSurfaceGanesh.h"
#include <objc/message.h>
#include <objc/runtime.h>
extern "C" id MTLCreateSystemDefaultDevice(void);
#endif
#include <cmath>
#include <cstdio>
#include <stdexcept>
#include <string_view>


using namespace canvas;
using namespace canvas::render;
namespace {
void require(bool b,const char* name){std::printf("{\"case\":\"%s\",\"pass\":%s}\n",name,b?"true":"false");if(!b)throw std::runtime_error(name);}
#if defined(__APPLE__)
id message(id target,const char* name){return reinterpret_cast<id(*)(id,SEL)>(objc_msgSend)(target,sel_registerName(name));}
struct NativeMetalCompletion final:GpuCompletionService {
    sk_sp<GrDirectContext> context;std::shared_ptr<GpuCompletionState> flags=std::make_shared<GpuCompletionState>();
    ~NativeMetalCompletion()override{context->flushAndSubmit(GrSyncCpu::kYes);context->checkAsyncWorkCompletion();flags->abandoned.store(true);}
    void service()noexcept override{context->checkAsyncWorkCompletion();}
    std::shared_ptr<const GpuCompletionState> state()const noexcept override{return flags;}
};
class NativeMetalTarget final:public SkiaSurfaceProvider {
    std::shared_ptr<NativeMetalCompletion> completion_;sk_sp<SkSurface> surface_;
    std::uint64_t generation_=0,presents_=0,reads_=0;
 public:
    explicit NativeMetalTarget(std::shared_ptr<NativeMetalCompletion> completion):completion_(std::move(completion)){}
    std::shared_ptr<GpuCompletionService> gpuCompletionService()const noexcept override{return unsupported?nullptr:completion_;}
    RenderTargetInfo describe()const noexcept override{RenderTargetInfo info;info.profileId="native-test-metal";info.kind=RenderTargetKind::kGpuOffscreen;info.backend=RenderTargetBackend::kMetal;info.capabilities.gpuAccelerated=true;info.capabilities.supportsReadback=true;info.capabilities.supportsPresent=true;info.capabilities.supportsPartialUpdate=true;return info;}
    SkiaSurfaceAcquireResult acquire()noexcept override{if(temporarilyUnavailable)return SkiaSurfaceAcquireResult::rejected(SkiaSurfaceAcquireCode::kUnavailable,"injected temporarily withheld target");return surface_?SkiaSurfaceAcquireResult::acquired({surface_.get(),generation_}):SkiaSurfaceAcquireResult{};}
    bool abandonOnRelease=false,failPresent=false,unsupported=false,temporarilyUnavailable=false;
    void release()noexcept override{if(abandonOnRelease){completion_->context->abandonContext();completion_->flags->abandoned=true;}}
    BackendSubmissionResult present()noexcept override{if(failPresent)return BackendSubmissionResult::rejected("injected provider present failure");completion_->context->flushAndSubmit(GrSyncCpu::kNo);++presents_;return BackendSubmissionResult::accepted();}
    BackendSubmissionResult resize(std::uint32_t w,std::uint32_t h)noexcept override{surface_=SkSurfaces::RenderTarget(completion_->context.get(),skgpu::Budgeted::kNo,SkImageInfo::MakeN32Premul(w,h));++generation_;return surface_?BackendSubmissionResult::accepted():BackendSubmissionResult::rejected("Metal test target unavailable");}
    BackendSubmissionResult readbackRgba(std::span<std::uint8_t> out)noexcept override{++reads_;auto info=SkImageInfo::Make(surface_->width(),surface_->height(),kRGBA_8888_SkColorType,kPremul_SkAlphaType);return surface_->readPixels(info,out.data(),info.minRowBytes(),0,0)?BackendSubmissionResult::accepted():BackendSubmissionResult::rejected("Metal test readback failed");}
    std::uint64_t generation()const noexcept override{return generation_;}
    BackendSubmissionResult advanceGeneration()noexcept override{++generation_;return BackendSubmissionResult::accepted();}
    std::uint64_t readbackCount()const noexcept override{return reads_;}
    std::uint64_t cpuCopyCount()const noexcept override{return reads_;}
    std::uint64_t presentCount()const noexcept override{return presents_;}
};
FramePlan renderPlan(const ReferenceTraversalEntry& entry,std::uint64_t id,unsigned dpr,std::uint64_t surfaceGeneration=1){
    FrameState f{ViewId{1},CameraState{{128,64},1,0,CameraGeneration{1}},{0,0,256,128},
        {256,128,256*dpr,128*dpr,float(dpr),1},semantic::SemanticGeneration{1},SceneRevision{1},SurfaceGeneration{surfaceGeneration},MetricsGeneration{1},FrameId{id},{0,0,256,128}};
    ReferenceDrawList draw{f};draw.viewportClip={0,0,256,128};
    semantic::ObjectRecord backdrop;backdrop.id=ObjectId::fromUint64(99);backdrop.kind=semantic::ObjectKind::kShape;backdrop.kind_version=1;
    backdrop.content=semantic::ShapeContent{1,256,128};backdrop.placement.order_key=semantic::OrderKey({1});
    backdrop.properties.entries={{0x100U,semantic::FillStyleValue{semantic::SolidFill{{0,0,1,1}}}}};
    const auto projected=projectRuntimeScene({&backdrop,1},semantic::SemanticGeneration{1});
    draw.entries={ReferenceTraversalEntry{projected.records.front(),true,ShapeReferenceCommand{std::get<semantic::ShapeContent>(backdrop.content)}},entry};
    return FramePlan{f,std::move(draw),{},false,true};
}
ReferenceTraversalEntry stroke(){
    semantic::CompactStrokeContent c;c.stroke.config.version=2;c.stroke.config.algorithmVersion=4;
    c.stroke.config.signalVersion=3;c.stroke.config.size=18;c.stroke.config.smoothing=.5;c.stroke.seed=42;
    for(unsigned i=0;i<128;++i)c.stroke.knots.push_back({20.+i,64+25*std::sin(i*.06),32768});
    ReferenceTraversalEntry e;e.record.objectId=ObjectId::fromUint64(1);e.record.content=std::move(c);
    e.command=CompactStrokeReferenceCommand{scene::ImmutableValue<semantic::CompactStrokeContent>::alias(e.record.content)};return e;
}
std::shared_ptr<NativeMetalCompletion> context(){
    const auto device=MTLCreateSystemDefaultDevice(),queue=message(device,"newCommandQueue");
    GrMtlBackendContext backend{sk_cfp<GrMTLHandle>(device),sk_cfp<GrMTLHandle>(queue)};
    auto c=std::make_shared<NativeMetalCompletion>();c->context=GrDirectContexts::MakeMetal(backend);
    require(bool(c->context),"actual_Metal_context");return c;
}
#endif
}
int main(){try{
#if defined(__APPLE__)
 for(unsigned dpr:{1U,2U,4U}){
    auto c=context();NativeMetalTarget p(c);SkiaRenderer r;
    require(p.resize(256*dpr,128*dpr).code==BackendSubmissionCode::kAccepted,"physical_target");
    const auto source=stroke();auto frame=renderPlan(source,1,dpr,p.generation());
    auto result=r.renderFrame(p,frame);
    for(unsigned i=0;result.code==BackendSubmissionCode::kRetry&&i<1024;++i)result=r.renderFrame(p,frame);
    require(result.code==BackendSubmissionCode::kAccepted&&p.presentCount()==1,"actual_canonical_submission");
    std::vector<std::uint8_t> before(std::size_t(256*dpr)*128*dpr*4);
    require(p.readbackRgba(before).code==BackendSubmissionCode::kAccepted,"cold_target_pixels");
    for(unsigned i=0;i<256&&r.resourceRetirementPending();++i){c->context->checkAsyncWorkCompletion();r.serviceResourceRetirements();}
    require(!r.resourceRetirementPending(),"idle_retirement_converges_without_edit");
    NavigationSourceQuery query=[&](const foundation::WorldRect&,std::vector<ReferenceTraversalEntry>& out){out=frame.referenceDrawList.entries;return true;};
    for(unsigned i=0;i<8&&!r.navigationWork().preparedGeometryBytes;++i)(void)r.renderNavigation(p,{1,0,0},frame.frame,query);
    const auto progress=r.navigationWork();require(progress.preparedGeometryBytes>0,"actual_navigation_retained_world_preparation");
    p.temporarilyUnavailable=true;
    require(r.renderNavigation(p,{1,0,0},frame.frame,query).code==BackendSubmissionCode::kRetry,"temporary_target_denial");
    require(r.navigationWork().preparedGeometryBytes==progress.preparedGeometryBytes,"temporary_target_denial_keeps_navigation_owner");
    p.temporarilyUnavailable=false;
    const auto lifetime=c->flags->lifetime,old=p.generation(),presentsBeforeResize=p.presentCount();
    require(p.resize(256*dpr,128*dpr).code==BackendSubmissionCode::kAccepted&&c->flags->lifetime==lifetime,"logical_resize_preserves_context_lifetime");
    require(r.renderFrame(p,frame).code==BackendSubmissionCode::kRejected&&p.presentCount()==presentsBeforeResize,"old_surface_generation_never_presents");
    const auto rebound=renderPlan(source,2,dpr,p.generation());
    result=r.renderFrame(p,rebound);
    for(unsigned i=0;result.code==BackendSubmissionCode::kRetry&&i<1024;++i)result=r.renderFrame(p,rebound);
    require(result.code==BackendSubmissionCode::kAccepted&&p.generation()>old,"rebind_draws_current_generation");
    std::vector<std::uint8_t> after(before.size());require(p.readbackRgba(after).code==BackendSubmissionCode::kAccepted&&before==after,"resize_preserves_canonical_pixels");
    p.failPresent=true;const auto count=p.presentCount();
    require(r.renderFrame(p,rebound).code==BackendSubmissionCode::kRejected&&p.presentCount()==count,"present_failure_not_success");
    p.failPresent=false;
    PreviewGeometry preview;preview.revision=1;preview.surfaceGeneration=SurfaceGeneration{p.generation()};
    preview.logicalToPhysicalX=preview.logicalToPhysicalY=float(dpr);preview.outline={{10,10},{30,10},{30,30},{10,30}};
    require(r.renderPreview(p,preview).code==BackendSubmissionCode::kAccepted,"preview_actual_GPU_lifetime");
    require(r.clearPreview(p,preview.surfaceGeneration).code==BackendSubmissionCode::kAccepted,"preview_clear_actual_GPU_lifetime");
    std::vector<std::uint8_t> empty(before.size());require(p.readbackRgba(empty).code==BackendSubmissionCode::kAccepted&&std::all_of(empty.begin(),empty.end(),[](auto x){return x==0;}),"preview_clear_transparent_pixels");
 }
 {
    auto c=context();NativeMetalTarget p(c);SkiaRenderer r;
    require(p.resize(256,128).code==BackendSubmissionCode::kAccepted,"bounded_world_progress_target");
    auto e=stroke();e.record.content.update([](auto& content){auto& stroke=std::get<semantic::CompactStrokeContent>(content).stroke;
      stroke.knots.clear();for(unsigned i=0;i<7199;++i)stroke.knots.push_back({20.+i*.02,64+25*std::sin(i*.006),32768});});
    e.command=CompactStrokeReferenceCommand{scene::ImmutableValue<semantic::CompactStrokeContent>::alias(e.record.content)};
    const auto first=renderPlan(e,5,1,p.generation());RenderPreparationBudget one;one.maxCoverageSteps=1;
    for(unsigned i=0;i<20;++i)require(r.renderFrame(p,first,nullptr,&one).code==BackendSubmissionCode::kRetry,"bounded_cold_pending");
    require(r.renderCacheWork().preparedSupportKnots>0&&p.presentCount()==0,"pending_world_preparation_no_partial_present");
    r.serviceResourceRetirements(ResourceServiceMode::kBeginSurfaceTransition);
    r.clearRenderCache();require(p.resize(256,128).code==BackendSubmissionCode::kAccepted,"world_progress_surface_rebind");
    r.serviceResourceRetirements(ResourceServiceMode::kEndSurfaceTransition);
    const auto next=renderPlan(e,6,1,p.generation());auto result=BackendSubmissionResult::retry("begin");
    for(unsigned i=0;i<4096&&result.code==BackendSubmissionCode::kRetry;++i)result=r.renderFrame(p,next,nullptr,&one);
    const auto work=r.renderCacheWork();
    require(result.code==BackendSubmissionCode::kAccepted&&p.presentCount()==1,"bounded_world_preparation_resumes_after_surface_rebind");
    require(work.preparedSupportKnots==7199&&work.preparedCompileInputs==7199&&work.preparedBodyBuilds==1,"world_progress_not_restarted_by_surface_loss");
 }
 {
    auto c=context();NativeMetalTarget p(c);SkiaRenderer r;
    require(p.resize(256,128).code==BackendSubmissionCode::kAccepted,"submit_failure_target");
    p.abandonOnRelease=true;
    const auto result=r.renderFrame(p,renderPlan(stroke(),3,1,p.generation()));
    require(result.code==BackendSubmissionCode::kRejected&&p.presentCount()==0,"failed_submit_precedes_provider_present_result");
    require(!c->flags->physicalLost.load(),"injected_abandon_not_physical_loss");
 }
#else
 throw std::runtime_error("frozen actual native Metal provider unavailable");
#endif
 return 0;
}catch(const std::exception& e){std::fprintf(stderr,"%s\n",e.what());return 1;}}
