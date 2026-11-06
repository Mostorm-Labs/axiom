#include "canvas/render/skia_renderer.hpp"
#include "canvas/scene/scene_types.hpp"
#include "canvas/ink/brush_package_catalog.hpp"
#include "../src/skia_navigation_tiles.hpp"
#include "../src/skia_render_cache_internal.hpp"
#include "include/core/SkCanvas.h"
#include <type_traits>
#include <iostream>
#include <stdexcept>
#if defined(G5_TEST_WEBGL)
#include "exact_contour_replay_oracle.hpp"
#include "navigation_alpha_mask_oracle.hpp"
#include "exact_contour_locality_oracle.hpp"
#include "reference_draw_stream_oracle.hpp"
#include "canvas/render/webgl_surface_backend.hpp"
#include "include/core/SkSurface.h"
#include "include/core/SkCanvas.h"
#include "include/core/SkColorSpace.h"
#include "../src/compact_raster_internal.hpp"
#include "../src/skia_prepared_geometry.hpp"
#include "../src/skia_gpu_lifetime.hpp"
#include <emscripten.h>
#include <emscripten/html5.h>
#endif

using namespace canvas;
using namespace canvas::render;
namespace {
void require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
int finishNative(int code){
#if defined(G5_TEST_WEBGL)
    EM_ASM({window.result.code=$0;window.result.nativeDone=true;},code);
#endif
    return code;
}
#if defined(G5_TEST_WEBGL)
// Test-owned browser App. Production readback remains disabled; this reads
// pixels only outside the measured loop to compare the actual GPU targets.
class TestSurface final : public SkiaSurfaceProvider {
    std::unique_ptr<WebGlSurfaceProvider> provider_;
    std::uint64_t generation_=0,presents_=0;
    bool lost_=false;
    EMSCRIPTEN_WEBGL_CONTEXT_HANDLE nativeContext_=0;
  public:
    void forgetAppContextForTest(){if(nativeContext_)emscripten_webgl_destroy_context(nativeContext_);nativeContext_=0;}
    bool completionSupported=true;
    std::shared_ptr<GpuCompletionService> gpuCompletionService()const noexcept override{return completionSupported&&provider_?provider_->gpuCompletionService():nullptr;}
    RenderTargetInfo describe()const noexcept override{return provider_->describe();}
    SkiaSurfaceAcquireResult acquire()noexcept override{
        if(lost_)return SkiaSurfaceAcquireResult::rejected(SkiaSurfaceAcquireCode::kLost,"lost");
        auto r=provider_->acquire();if(r.code==SkiaSurfaceAcquireCode::kAcquired)r.frame.generation=generation_;return r;}
    void release()noexcept override{provider_->release();}
    BackendSubmissionResult present()noexcept override{++presents_;return provider_->present();}
    BackendSubmissionResult lose()noexcept override{lost_=true;return BackendSubmissionResult::accepted();}
    BackendSubmissionResult resize(std::uint32_t width,std::uint32_t height)noexcept override{
        if(!provider_){
            static unsigned next=0;const auto id=++next;
            EM_ASM({const c=document.createElement('canvas');c.id='cache-'+$0;c.width=$1;c.height=$2;document.body.append(c);},id,width,height);
            const auto selector=std::string("#cache-")+std::to_string(id);
            EmscriptenWebGLContextAttributes a;emscripten_webgl_init_context_attributes(&a);
            a.majorVersion=2;a.alpha=true;a.antialias=false;a.depth=false;a.stencil=true;a.premultipliedAlpha=true;a.preserveDrawingBuffer=true;
            const auto context=emscripten_webgl_create_context(selector.c_str(),&a);
            if(context<=0||emscripten_webgl_make_context_current(context)!=EMSCRIPTEN_RESULT_SUCCESS)return BackendSubmissionResult::rejected("GPU test context");
            nativeContext_=context;provider_=WebGlSurfaceProvider::fromCurrentContext(width,height);
        }
        ++generation_;lost_=false;return provider_&&provider_->ready()?BackendSubmissionResult::accepted():BackendSubmissionResult::rejected("GPU test provider");}
    BackendSubmissionResult readbackRgba(std::span<std::uint8_t> data)noexcept override{
        const auto a=acquire();if(a.code!=SkiaSurfaceAcquireCode::kAcquired)return BackendSubmissionResult::rejected("readback acquire");
        const auto info=SkImageInfo::Make(256,128,kRGBA_8888_SkColorType,kPremul_SkAlphaType,SkColorSpace::MakeSRGB());
        const bool ok=a.frame.surface->readPixels(info,data.data(),256*4,0,0);release();
        return ok?BackendSubmissionResult::accepted():BackendSubmissionResult::rejected("GPU test readback");}
    std::uint64_t generation()const noexcept override{return generation_;}
    BackendSubmissionResult advanceGeneration()noexcept override{++generation_;return BackendSubmissionResult::accepted();}
    std::uint64_t readbackCount()const noexcept override{return 0;}
    std::uint64_t cpuCopyCount()const noexcept override{return 0;}
    std::uint64_t presentCount()const noexcept override{return presents_;}
};
#else
using TestSurface=RasterSkiaSurfaceProvider;
#endif
FramePlan plan(std::uint64_t frameId, std::vector<ReferenceTraversalEntry> entries,
               std::uint64_t generation = 1, WorldToViewAffine transform = {},
               foundation::WorldRect clip = {0,0,256,128},
               foundation::DocumentBackground background = {}, bool transparent=false, float dpr=1) {
    FrameState frame{ViewId{1}, CameraState{{0,0},1,0,CameraGeneration{1}},
        {0,0,256,128}, {256/dpr,128/dpr,256,128,dpr,dpr}, semantic::SemanticGeneration{frameId},
        SceneRevision{frameId}, SurfaceGeneration{generation}, MetricsGeneration{1}, FrameId{frameId}};
    ReferenceDrawList list{frame}; list.viewportClip=clip;
    list.worldToView=transform; list.entries=std::move(entries);
    return FramePlan{frame, std::move(list),background,transparent};
}
ReferenceTraversalEntry stroke(std::uint64_t id, double y=40) {
    semantic::ObjectRecord object; object.id=ObjectId::fromUint64(id);
    object.kind=semantic::ObjectKind::kVectorStroke; object.kind_version=3;
    foundation::InkBrushConfig config; config.size=24; config.opacity=.55;
    object.content=semantic::CompactStrokeContent{{config,id,{{20,y,32768},{225,y+20,48000}}}};
    auto projection=projectRuntimeScene({&object,1},semantic::SemanticGeneration{1});
    return {projection.records.front(),true,CompactStrokeReferenceCommand{std::get<semantic::CompactStrokeContent>(object.content)}};
}
std::vector<std::uint8_t> pixels(SkiaSurfaceProvider& provider) {
    std::vector<std::uint8_t> output(256*128*4);
    require(provider.readbackRgba(output).code==BackendSubmissionCode::kAccepted,"readback");
    return output;
}
// Real browser GPU completion requires an event-loop turn between owner ticks.
// Keep that yield in the private harness; production never sleeps or waits.
template<class... Args>
BackendSubmissionResult navigate(SkiaRenderer& renderer, SkiaSurfaceProvider& provider,
    const CanonicalViewportTransform& camera, Args&&... args) {
#if defined(G5_TEST_WEBGL)
    emscripten_sleep(1);
#endif
    return renderer.renderNavigation(provider,camera,std::forward<Args>(args)...);
}
class FailingPresent final : public SkiaSurfaceProvider {
  public:
    TestSurface raster;bool fail=true;
    std::shared_ptr<GpuCompletionService> gpuCompletionService()const noexcept override{return raster.gpuCompletionService();}
    RenderTargetInfo describe()const noexcept override{return raster.describe();}
    SkiaSurfaceAcquireResult acquire()noexcept override{return raster.acquire();}
    void release()noexcept override{raster.release();}
    BackendSubmissionResult present()noexcept override{return fail?BackendSubmissionResult::retry("injected present failure"):raster.present();}
    BackendSubmissionResult resize(std::uint32_t w,std::uint32_t h)noexcept override{return raster.resize(w,h);}
    BackendSubmissionResult readbackRgba(std::span<std::uint8_t> d)noexcept override{return raster.readbackRgba(d);}
    std::uint64_t generation()const noexcept override{return raster.generation();}
    BackendSubmissionResult advanceGeneration()noexcept override{return raster.advanceGeneration();}
    std::uint64_t readbackCount()const noexcept override{return raster.readbackCount();}
    std::uint64_t cpuCopyCount()const noexcept override{return raster.cpuCopyCount();}
    std::uint64_t presentCount()const noexcept override{return raster.presentCount();}
};
}
void checkRetainedBackground(TestSurface& target,TestSurface& reference){
    auto entry=stroke(82001);auto c=std::get<CompactStrokeReferenceCommand>(entry.command).content.get();
    c.stroke.knots={{80,60,32768},{150,70,32768}};std::get<CompactStrokeReferenceCommand>(entry.command).content=c;entry.record.content=c;
    entry.record.worldBounds={32,12,198,118};const auto frame=plan(820,{entry});
    SkiaRenderer cached,direct;RenderCacheOptions options;options.navigationMaxSteps=1;options.navigationMicroseconds=1000000;
    cached.setRenderCacheOptions(options);RenderCacheOptions disabled;disabled.enabled=false;direct.setRenderCacheOptions(disabled);
    require(cached.renderFrame(target,frame).code==BackendSubmissionCode::kAccepted,"background proof source");
    NavigationSourceQuery sources=[&](const foundation::WorldRect&,std::vector<ReferenceTraversalEntry>& out){out={entry};return true;};
    unsigned proofs=0;NavigationBackgroundQuery background=[&](const foundation::WorldRect& region){++proofs;return !region.intersects(entry.record.worldBounds);};
    const auto presents=target.presentCount();
    require(navigate(cached,target,{1,8,4},frame.frame,sources,false,nullptr,background).code==BackendSubmissionCode::kAccepted&&
            target.presentCount()==presents+1&&cached.navigationWork().presented,
            "known-empty exposed border held navigation behind tile preparation");
    require(proofs>0&&proofs<=4&&cached.submissionCount()==1,"background proof exceeded query cap or emitted canonical receipt");
    const auto feedback=pixels(target);
    require(direct.renderFrame(reference,plan(821,{entry},1,{1,0,0,1,8,4})).code==BackendSubmissionCode::kAccepted&&feedback==pixels(reference),
            "empty-border reprojection changed translated pixels");
    const auto zoomPresents=target.presentCount(),zoomQueries=cached.navigationWork().queries;
    require(navigate(cached,target,{.9F,12.8F,6.4F},frame.frame,sources,false,nullptr,background).code==BackendSubmissionCode::kAccepted&&
            target.presentCount()==zoomPresents+1&&cached.navigationWork().queries-zoomQueries<=4,
            "four empty zoom-out borders lost immediate complete feedback or exceeded query cap");
    SkiaRenderer guarded;options.navigationMaxQueries=1;guarded.setRenderCacheOptions(options);
    require(guarded.renderFrame(target,frame).code==BackendSubmissionCode::kAccepted,"negative background source");
    unsigned failures=0;NavigationBackgroundQuery unknown=[&](const foundation::WorldRect&){++failures;return false;};
    const auto before=target.presentCount();
    for(unsigned i=0;;++i){const auto queries=guarded.navigationWork().queries;
        require(navigate(guarded,target,{1,8,0},frame.frame,sources,false,nullptr,unknown).code==BackendSubmissionCode::kAccepted,"unknown exposure fallback");
        require(guarded.navigationWork().queries-queries<=1,"background proof bypassed shared query budget");
        if(i==0)require(target.presentCount()==before,"unknown exposure falsely certified complete viewport");
        if(!guarded.navigationWork().pending)break;require(i<127,"failed background proof starved tile convergence");
    }
    require(failures==1,"failed exposure proof repeated and starved coverage");
}
void checkExactCachePressure(TestSurface& target,TestSurface& reference){
    auto entry=stroke(8391);const auto frame=plan(839,{entry});
    SkiaRenderer cached,direct;RenderCacheOptions options;
    options.maxBytes=4*1024*1024;options.maxEntryBytes=options.maxBytes;
    options.navigationMaxSteps=4096;options.navigationMicroseconds=1000000;
    cached.setRenderCacheOptions(options);RenderCacheOptions disabled;disabled.enabled=false;direct.setRenderCacheOptions(disabled);
    require(cached.renderFrame(target,frame).code==BackendSubmissionCode::kAccepted,"pressure source frame");
    NavigationSourceQuery query=[&](const foundation::WorldRect&,std::vector<ReferenceTraversalEntry>& out){out={entry};return true;};
    for(unsigned i=0;;++i){
        require(navigate(cached,target,{1,8,0},frame.frame,query).code==BackendSubmissionCode::kAccepted,"pressure navigation");
        if(!cached.navigationWork().pending)break;
        require(i<128,"pressure navigation did not converge");
    }
    require(cached.navigationWork().retainedBytes>0,"pressure test did not retain world tiles");
    const auto evictions=cached.navigationWork().evictions;
    const auto exact=plan(840,{entry},1,{1,0,0,1,8,0});RenderPreparationBudget budget;
    for(unsigned i=0;;++i){
        const auto result=cached.renderFrame(target,exact,nullptr,&budget);
        if(result.code==BackendSubmissionCode::kAccepted)break;
        require(result.code==BackendSubmissionCode::kRetry&&i<128,"pressure exact did not converge");
    }
    require(cached.navigationWork().evictions==evictions&&cached.navigationWork().retainedBytes>0,
            "exact preparation discarded useful navigation tiles without actual admission pressure");
    require(direct.renderFrame(reference,exact).code==BackendSubmissionCode::kAccepted&&pixels(target)==pixels(reference),"pressure eviction changed exact pixels");
    const auto presents=target.presentCount();
    require(navigate(cached,target,{1,16,0},exact.frame,query,false,nullptr,
            NavigationBackgroundQuery{[](const foundation::WorldRect&){return true;}}).code==BackendSubmissionCode::kAccepted&&
            target.presentCount()==presents+1,"exact pressure eviction lost retained-image navigation feedback");
}
void checkCompletionProviderLifetime(TestSurface& target){
#if defined(G5_TEST_WEBGL)
    SkiaRenderer renderer;auto owned=std::make_unique<TestSurface>();
    require(owned->resize(256,128).code==BackendSubmissionCode::kAccepted,"retired context setup");
    std::weak_ptr<GpuCompletionService> context=owned->gpuCompletionService();
    require(!context.expired()&&renderer.renderFrame(*owned,plan(8391,{stroke(8391)})).code==BackendSubmissionCode::kAccepted,"retired context frame");
    renderer.clearRenderCache();owned.reset();
    require(!context.expired(),"provider destruction abandoned unfinished tracked context");
    for(unsigned i=0;i<128&&!context.expired();++i){emscripten_sleep(1);renderer.serviceResourceRetirements();}
    require(context.expired(),"replaced provider completion context leaked");
    target.completionSupported=false;SkiaRenderer unsupported;
    require(unsupported.renderFrame(target,plan(8391,{stroke(8391)})).code==BackendSubmissionCode::kAccepted,"unsupported provider lost Direct fallback");
    require(unsupported.renderCacheWork().retainedBytes==0,"unsupported GPU completion admitted optional cache");
    const auto frame=plan(8391,{stroke(8391)});
    NavigationSourceQuery query=[&](const foundation::WorldRect&,std::vector<ReferenceTraversalEntry>& out){out=frame.referenceDrawList.entries;return true;};
    require(unsupported.renderNavigation(target,{1,0,0},frame.frame,query).code==BackendSubmissionCode::kRetry,"unsupported provider accepted unsafe navigation cache");
    target.completionSupported=true;
    auto missing=std::make_unique<TestSurface>();require(missing->resize(256,128).code==BackendSubmissionCode::kAccepted,"missing context setup");
    auto abandoned=missing->gpuCompletionService();missing->forgetAppContextForTest();abandoned->service();
    require(abandoned->state()->abandoned.load()&&!abandoned->state()->physicalLost.load(),"deleted app handle masqueraded as physical GPU completion");
#else
    (void)target;
#endif
}
void checkCoverageReclaimFeasibility(TestSurface& target){
    checkCompletionProviderLifetime(target);
    constexpr std::size_t mib=1024*1024;
    auto entry=stroke(8392);auto content=std::get<CompactStrokeReferenceCommand>(entry.command).content.get();
    content.stroke.config=*ink::BrushPackageCatalog{}.loadDefault("round-handdrawn",4).package.compactConfig;
    entry.record.content=content;entry.command=CompactStrokeReferenceCommand{content};
    const auto frame=plan(8392,{entry});
    for(auto pressureClass:{resources::ResourceClass::kRecorded,resources::ResourceClass::kScratch})for(bool feasible:{false,true}){
        internal::SkiaNavigationTiles nav;
        auto retirement=std::make_shared<internal::GpuResourceRetirement>();
        RenderCacheOptions options;options.maxBytes=4*mib;options.maxEntryBytes=options.maxBytes;
        options.navigationMaxSteps=64;options.navigationMicroseconds=1000000;
        auto budget=std::make_shared<resources::ResourceBudgetManager>(resources::BudgetLimits{4*mib,12*mib});
        NavigationSourceQuery query=[&](const foundation::WorldRect&,std::vector<ReferenceTraversalEntry>& out){out={entry};return true;};
        for(unsigned i=0;;++i){
#if defined(G5_TEST_WEBGL)
            emscripten_sleep(1);
#endif
            require(nav.render(target,{1,0,0},frame.frame,query,1,options,budget,false,nullptr,nullptr,nullptr,{}, {},retirement).code==BackendSubmissionCode::kAccepted,"coverage reclaim setup");
            if(!nav.work.pending)break;require(i<256,"coverage reclaim setup stalled");
        }
        (void)pixels(target);retirement->service(); // private oracle drain, never production wait
#if defined(G5_TEST_WEBGL)
        for(unsigned i=0;i<128&&budget->pendingGpuEpochCount();++i){emscripten_sleep(1);retirement->service();}
#endif
        const auto usage=budget->snapshot(resources::ResourceClass::kNavigationTile);
        require(usage.staging==0&&usage.pendingRelease==0,"completed navigation retained transient staging charge");
        nav.work.retainedBytes=usage.used;
        const auto retained=nav.work.retainedBytes;const auto evictions=nav.work.evictions;
        require(retained>0,"coverage reclaim fixture retained no work");
        const auto pressure=budget->reserve(pressureClass,(feasible?4*mib+4096-budget->used():4*mib),true);
        require(bool(pressure),"coverage pressure fixture reservation");
        resources::ResourceLease lease(budget,*pressure);
        nav.reclaimForExact(budget,4*mib,4096,frame.frame,&target,1);
        if(feasible){
            require(nav.work.evictions>evictions&&budget->used()<=4*mib-4096,"feasible exact admission was starved by coverage retention");
            const auto admitted=budget->reserve(resources::ResourceClass::kRecorded,4096,false);
            require(bool(admitted),"necessary reclaim did not admit optional recording");
            budget->release(*admitted);
            const auto masks=nav.work.coarseMaskStarts,built=nav.work.tilesBuilt;
            options.navigationMaxSteps=1;
#if defined(G5_TEST_WEBGL)
            emscripten_sleep(1);
#endif
            require(nav.render(target,{1,-8,0},frame.frame,query,1,options,budget,false,nullptr,nullptr,nullptr,{}, {},retirement).code==BackendSubmissionCode::kAccepted&&nav.work.presented,
                "optional recording discarded the completed frontier needed by the next camera demand");
            require(nav.work.coarseMaskStarts==masks&&nav.work.tilesBuilt==built,"feasible reclaim forced a completed frontier to be rebuilt");
        }else{
            require(nav.work.evictions==evictions&&nav.work.retainedBytes==retained,"impossible optional admission discarded reusable coverage");
            const auto masks=nav.work.coarseMaskStarts,built=nav.work.tilesBuilt;
            require(masks>0,"reclaim reuse fixture never built coarse coverage");
            options.navigationMaxSteps=1;
#if defined(G5_TEST_WEBGL)
            emscripten_sleep(1);
#endif
            require(nav.render(target,{2,0,0},frame.frame,query,1,options,budget,false,nullptr,nullptr,nullptr,{}, {},retirement).code==BackendSubmissionCode::kAccepted&&nav.work.presented,
                "next camera demand could not present preserved complete coverage");
            require(nav.work.coarseMaskStarts==masks&&nav.work.tilesBuilt==built,"next camera demand repeated completed coarse coverage");
        }
        const auto foreign=std::make_shared<resources::ResourceBudgetManager>();
        const auto foreignToken=foreign->reserve(resources::ResourceClass::kScratch,4*mib,true);
        require(bool(foreignToken),"foreign reclaim fixture reservation");
        resources::ResourceLease foreignLease(foreign,*foreignToken);
        const auto beforeForeign=nav.work.retainedBytes;const auto evictedBeforeForeign=nav.work.evictions;
        nav.reclaimForExact(foreign,4*mib,4096,frame.frame,&target,1);
        require(nav.work.retainedBytes==beforeForeign&&nav.work.evictions==evictedBeforeForeign,"foreign budget discarded completed coverage");
    }
    // A bounded cache can contain the old coarse frontier, its parent and the
    // current sharp cell. Optional prewarm must not trade the frontier away.
    internal::SkiaNavigationTiles nav;
        auto retirement=std::make_shared<internal::GpuResourceRetirement>();
    RenderCacheOptions options;options.maxBytes=4*mib;options.maxEntries=3;
    options.navigationMaxSteps=64;options.navigationMicroseconds=1000000;
    auto budget=std::make_shared<resources::ResourceBudgetManager>(resources::BudgetLimits{4*mib,12*mib});
    NavigationSourceQuery query=[&](const foundation::WorldRect&,std::vector<ReferenceTraversalEntry>& out){out={entry};return true;};
    for(auto camera:{CanonicalViewportTransform{1,0,0},CanonicalViewportTransform{2,0,0}}){
        for(unsigned i=0;;++i){
#if defined(G5_TEST_WEBGL)
            emscripten_sleep(1);
#endif
            require(nav.render(target,camera,frame.frame,query,1,options,budget,false,nullptr,nullptr,nullptr,{}, {},retirement).code==BackendSubmissionCode::kAccepted,"prewarm reuse setup");
            if(!nav.work.pending)break;require(i<256,"prewarm reuse setup stalled");
        }
    }
    for(unsigned i=0;;++i){
        require(nav.render(target,{2,0,0},frame.frame,query,1,options,budget,true,nullptr,nullptr,nullptr,{}, {},retirement).code==BackendSubmissionCode::kAccepted,"prewarm reuse admission");
        if(!nav.work.pending)break;require(i<256,"prewarm reuse did not yield");
    }
    const auto masks=nav.work.coarseMaskStarts,built=nav.work.tilesBuilt;options.navigationMaxSteps=1;
    require(nav.render(target,{1,-8,0},frame.frame,query,1,options,budget,false,nullptr,nullptr,nullptr,{}, {},retirement).code==BackendSubmissionCode::kAccepted&&nav.work.presented,
        "optional prewarm evicted reusable frontier coverage");
    require(nav.work.coarseMaskStarts==masks&&nav.work.tilesBuilt==built,"prewarm forced frontier replay");
}
void checkNavigationSharpOverlap(TestSurface& target,TestSurface& reference){
    std::vector<ReferenceTraversalEntry> entries;
    for(unsigned i=0;i<56;++i){semantic::ObjectRecord o;o.id=ObjectId::fromUint64(91000+i);
        o.kind=semantic::ObjectKind::kShape;o.kind_version=1;o.content=semantic::ShapeContent{1,1,96};
        o.transform.tx=16+i*4;o.transform.ty=16;
        const auto projection=projectRuntimeScene({&o,1},semantic::SemanticGeneration{1});
        entries.push_back({projection.records.front(),true,ShapeReferenceCommand{std::get<semantic::ShapeContent>(o.content)}});
    }
    const auto frame=plan(910,entries);SkiaRenderer cached,direct;RenderCacheOptions options;
    options.navigationMaxSteps=1;options.navigationMicroseconds=1000000;options.navigationMaxQueries=4;
    cached.setRenderCacheOptions(options);RenderCacheOptions disabled;disabled.enabled=false;direct.setRenderCacheOptions(disabled);
    require(cached.renderFrame(target,frame).code==BackendSubmissionCode::kAccepted,"sharp overlap source");
    require(direct.renderFrame(reference,plan(911,entries,1,{1,0,0,1,8,0})).code==BackendSubmissionCode::kAccepted,"sharp overlap reference");
    const auto expected=pixels(reference);NavigationSourceQuery query=[&](const foundation::WorldRect&,std::vector<ReferenceTraversalEntry>& out){out=entries;return true;};
    for(unsigned i=0;i<256;++i){
        require(navigate(cached,target,{1,8,0},frame.frame,query,false,nullptr,
                NavigationBackgroundQuery{[](const foundation::WorldRect&){return true;}}).code==BackendSubmissionCode::kAccepted,"sharp overlap navigation");
        if(cached.navigationWork().presented)require(pixels(target)==expected,"coarse coverage replaced sharper retained content during pan");
        if(!cached.navigationWork().pending)break;
        require(i<255,"sharp overlap refinement did not converge");
    }
    require(cached.submissionCount()==1,"navigation quality emitted a canonical submission");
}
// A populated frontier cannot be certified by empty-background proof. Every
// submitted overlap retains its original sharp pixels while cold cells fill.
void checkNavigationColdFrontier(TestSurface& target,TestSurface& reference){
    std::vector<ReferenceTraversalEntry> entries;
    for(unsigned i=0;i<68;++i){semantic::ObjectRecord o;o.id=ObjectId::fromUint64(92000+i);
        o.kind=semantic::ObjectKind::kShape;o.kind_version=1;o.content=semantic::ShapeContent{1,1,96};
        o.transform.tx=16+i*4;o.transform.ty=16;
        const auto projection=projectRuntimeScene({&o,1},semantic::SemanticGeneration{1});
        entries.push_back({projection.records.front(),true,ShapeReferenceCommand{std::get<semantic::ShapeContent>(o.content)}});
    }
    const auto frame=plan(920,entries);SkiaRenderer cached,direct;RenderCacheOptions options;
    options.navigationMaxSteps=1;options.navigationMicroseconds=1000000;cached.setRenderCacheOptions(options);
    RenderCacheOptions disabled;disabled.enabled=false;direct.setRenderCacheOptions(disabled);
    require(cached.renderFrame(target,frame).code==BackendSubmissionCode::kAccepted,"cold frontier source");
    NavigationSourceQuery query=[&](const foundation::WorldRect&,std::vector<ReferenceTraversalEntry>& out){out=entries;return true;};
    unsigned presents=0;double previousLow=1;
    const auto run=[&](CanonicalViewportTransform camera,bool compareOverlap){
        require(direct.renderFrame(reference,plan(921,entries,1,{camera.scale,0,0,camera.scale,camera.translationX,camera.translationY})).code==BackendSubmissionCode::kAccepted,"cold reference");
        const auto expected=pixels(reference);previousLow=1;
        for(unsigned i=0;;++i){
            const auto result=navigate(cached,target,camera,frame.frame,query,false,nullptr,
                NavigationBackgroundQuery{[](const foundation::WorldRect&){return false;}});
            require(result.code==BackendSubmissionCode::kAccepted,"cold frontier navigation");
            const auto work=cached.navigationWork();
            if(work.presented){++presents;require(work.coverageComplete,"partial cold viewport was presented");
                require(work.lowResolutionFraction<=previousLow+1e-6,"fixed-camera refinement regressed resolution");
                previousLow=work.lowResolutionFraction;
                if(compareOverlap){const auto actual=pixels(target);
                    for(unsigned y=0;y<128;++y)for(unsigned x=0;x<220;++x)for(unsigned c=0;c<4;++c)
                        require(actual[(y*256+x)*4+c]==expected[(y*256+x)*4+c],"cold frontier replaced sharp overlap");}
            }
            if(!work.pending)break;require(i<2048,"cold/zoom refinement did not converge");
        }
        require(cached.navigationWork().maximumUpscale<=1.25&&cached.navigationWork().lowResolutionFraction==0,
                "cold/zoom navigation never recovered adequate resolution");
        if(camera.scale==1)require(pixels(target)==expected,"refined integer-pan pixels differ from Direct");
        // Downsampling an existing high-density source can differ by one AA
        // quantization unit. Canonical equality is required after exact render,
        // not falsely attributed to a resampled navigation image.
        const auto canonical=plan(922,entries,1,{camera.scale,0,0,camera.scale,camera.translationX,camera.translationY});
        SkiaRenderer exact;require(exact.renderFrame(target,canonical).code==BackendSubmissionCode::kAccepted&&pixels(target)==expected,
            "post-navigation canonical pixels differ from Direct");
    };
    run({1,-32,0},true);
    run({2,-64,0},false); // cross the density level; no nearest/snap workaround
    run({.5F,-16,0},false);
    require(presents>2&&cached.navigationWork().levelChanges>=2&&cached.submissionCount()==1,
            "cold/zoom oracle missed levels or emitted canonical submission");
}
void checkNavigationRefinementProgress(TestSurface& target){
    std::vector<ReferenceTraversalEntry> entries;
    for(unsigned i=0;i<56;++i){semantic::ObjectRecord o;o.id=ObjectId::fromUint64(93000+i);
        o.kind=semantic::ObjectKind::kShape;o.kind_version=1;o.content=semantic::ShapeContent{1,2,96};
        o.transform.tx=240+i*5;o.transform.ty=16;
        const auto projection=projectRuntimeScene({&o,1},semantic::SemanticGeneration{1});
        entries.push_back({projection.records.front(),true,ShapeReferenceCommand{std::get<semantic::ShapeContent>(o.content)}});
    }
    SkiaRenderer cached;RenderCacheOptions options;options.navigationMaxSteps=1;options.navigationMicroseconds=1000000;
    cached.setRenderCacheOptions(options);const auto frame=plan(930,entries);
    NavigationSourceQuery query=[&](const foundation::WorldRect&,std::vector<ReferenceTraversalEntry>& out){out=entries;return true;};
    for(unsigned i=0;;++i){
        require(navigate(cached,target,{1,-250,0},frame.frame,query).code==BackendSubmissionCode::kAccepted,"refinement source");
        // One parent is complete; an applicable detail cell has real progress.
        if(cached.navigationWork().tilesBuilt==1&&cached.navigationWork().sourceSteps>=66)break;
        require(i<512,"detail job never started");
    }
    const auto before=cached.navigationWork();
    for(unsigned i=0;;++i){
        require(navigate(cached,target,{1,-258,0},frame.frame,query).code==BackendSubmissionCode::kAccepted,"frontier interruption");
        if(!cached.navigationWork().pending)break;require(i<2048,"suspended detail failed to converge");
    }
    const auto after=cached.navigationWork();
    require(after.suspendedRefinements>before.suspendedRefinements&&after.resumedRefinements>before.resumedRefinements,
            "cold frontier discarded instead of suspending applicable fine progress");
    require(after.cancelledJobs==before.cancelledJobs&&after.lowResolutionFraction==0,
            "frontier cancelled applicable detail or left persistent resolution debt");
    require(cached.submissionCount()==0,"derived refinement manufactured canonical receipt");
}
void checkAffineStrokes(TestSurface& target,TestSurface& reference){
    const semantic::Transform2D transforms[]={{1,0,0,1,12,8},{.9,.15,-.1,.8,10,5},{1.1,0,0,.75,-5,4}};
    for(const auto& transform:transforms)for(bool mixed:{false,true}){
        auto entry=stroke(81001);auto c=std::get<CompactStrokeReferenceCommand>(entry.command).content.get();
        c.stroke.config=*ink::BrushPackageCatalog{}.loadDefault("round-handdrawn",4).package.compactConfig;
        c.stroke.knots.clear();for(unsigned i=0;i<256;++i)c.stroke.knots.push_back({20.+i*.75,55.+std::sin(i*.17)*18,32768});
        std::get<CompactStrokeReferenceCommand>(entry.command).content=c;entry.record.content=c;entry.record.transform=transform;entry.record.worldBounds={0,0,256,128};
        auto entries=std::vector{entry};
        if(mixed){semantic::ObjectRecord o;o.id=ObjectId::fromUint64(81002);o.kind=semantic::ObjectKind::kShape;o.kind_version=1;
            o.content=semantic::ShapeContent{1,100,80};o.transform.tx=70;o.transform.ty=20;
            const auto projection=projectRuntimeScene({&o,1},semantic::SemanticGeneration{1});
            entries.insert(entries.begin(),{projection.records.front(),true,ShapeReferenceCommand{std::get<semantic::ShapeContent>(o.content)}});}
        const auto frame=plan(810,entries);SkiaRenderer cached,direct;RenderCacheOptions disabled;disabled.enabled=false;direct.setRenderCacheOptions(disabled);
        RenderPreparationBudget budget;budget.maxCoverageSteps=1;budget.maxGroups=1;budget.maxMicroseconds=1000000;
        const auto presents=target.presentCount();
        require(cached.renderFrame(target,frame,nullptr,&budget).code==BackendSubmissionCode::kRetry&&target.presentCount()==presents,
                "transformed stroke bypassed cooperative preparation");
        unsigned attempts=0;BackendSubmissionResult r;
        do{r=cached.renderFrame(target,frame,nullptr,&budget);require(++attempts<128,"affine preparation did not converge");}while(r.code==BackendSubmissionCode::kRetry);
        require(r.code==BackendSubmissionCode::kAccepted,"affine prepared frame rejected");
        const auto actual=pixels(target);
        require(direct.renderFrame(reference,frame).code==BackendSubmissionCode::kAccepted,"affine direct frame");
        const auto expected=pixels(reference);
        if(actual!=expected){unsigned n=0,max=0;for(std::size_t i=0;i<actual.size();++i){if(actual[i]!=expected[i]&&n<4)std::cerr<<"pixel "<<(i/4)%256<<','<<(i/4)/256<<" actual="<<int(actual[i])<<" expected="<<int(expected[i])<<'\n';n+=actual[i]!=expected[i];max=std::max(max,unsigned(std::abs(int(actual[i])-int(expected[i]))));}std::cerr<<"affine mixed="<<mixed<<" a="<<transform.a<<" b="<<transform.b<<" tx="<<transform.tx<<" differences="<<n<<" max="<<max<<'\n';}
        require(actual==expected,"affine cached/direct pixels differ");
        const auto rasterized=cached.renderCacheWork().rasterizedGroups;
        require(rasterized==1+std::uint64_t(mixed)&&cached.renderCacheWork().directGroups==std::uint64_t(mixed),"affine stroke still drew synchronously after preparation");
        require(cached.renderFrame(target,plan(811,entries),nullptr,&budget).code==BackendSubmissionCode::kAccepted&&
                cached.renderCacheWork().rasterizedGroups==rasterized+std::uint64_t(mixed)&&pixels(target)==actual,"affine warm raster not retained");
        SkiaRenderer nav;RenderCacheOptions options;options.navigationMicroseconds=1000000;nav.setRenderCacheOptions(options);
        NavigationSourceQuery query=[&](const foundation::WorldRect&,std::vector<ReferenceTraversalEntry>& out){out=entries;return true;};
        for(unsigned n=0;;++n){
            require(navigate(nav,target,{1,0,0},frame.frame,query).code==BackendSubmissionCode::kAccepted,"transformed stroke disabled navigation coverage");
            if(!nav.navigationWork().pending)break;require(n<127,"affine navigation stalled");
        }
        require(nav.navigationWork().coverageComplete&&nav.navigationWork().contourPrimitivesDrawn>0&&nav.submissionCount()==0,
                "affine navigation missing geometry or manufactured canonical receipt");
        const auto feedback=pixels(target);
        auto geometryOracle=actual;
#if defined(G5_TEST_WEBGL)
        // Navigation deliberately uses Ganesh coverage, whereas canonical v4
        // retains pinned CPU AA. Compare its geometry with an independent full
        // Ganesh contour stream, not a CPU/GPU edge-color equality assumption.
        auto background=entries;background.pop_back();
        require(direct.renderFrame(reference,plan(812,background)).code==BackendSubmissionCode::kAccepted,"affine geometry oracle background");
        const auto acquired=reference.acquire();auto* draw=acquired.frame.surface->getCanvas();
        auto coverage=draw->makeSurface(SkImageInfo::MakeN32Premul(256,128));
        require(bool(coverage),"affine GPU geometry oracle target");
        const auto matrix=SkMatrix::MakeAll(float(transform.a),float(transform.c),float(transform.tx),float(transform.b),float(transform.d),float(transform.ty),0,0,1);
        coverage->getCanvas()->clear(SK_ColorBLACK);coverage->getCanvas()->setMatrix(matrix);
        internal::drawCompactContourCoverageToSkCanvas(*coverage->getCanvas(),c.stroke.config,ink::compileCompactContour(c.stroke.config,c.stroke.knots));
        draw->save();draw->concat(matrix);
        internal::compositeCompactCoverageToSkCanvas(*draw,*coverage->makeImageSnapshot(),c.stroke.config,c.stroke.seed);
        draw->restore();reference.release();geometryOracle=pixels(reference);
#endif
        unsigned error=0;
        for(std::size_t i=0;i<feedback.size();++i)error=std::max(error,unsigned(std::abs(int(feedback[i])-int(geometryOracle[i]))));
        if(error>3)std::cerr<<"nav error="<<error<<" mixed="<<mixed<<" a="<<transform.a<<" b="<<transform.b<<" tx="<<transform.tx<<'\n';
        require(error<=3,"affine navigation did not match canonical geometry");
    }
}
#if defined(G5_TEST_WEBGL)
void checkGpuContours(TestSurface& provider){
    const auto acquired=provider.acquire();auto* canvas=acquired.frame.surface->getCanvas();
    const auto info=SkImageInfo::MakeN32Premul(600,300);
    // Local contour selection must match complete paired paths on
    // the actual Ganesh target as well as the native raster oracle.
    internal::PreparedGeometryCache geometry;
    auto geometryBudget=std::make_shared<resources::ResourceBudgetManager>();
    for(const auto profile:{"round-handdrawn","marker-compact-v1"}){
        auto entry=stroke(889);auto content=std::get<CompactStrokeReferenceCommand>(entry.command).content.get();
        content.stroke.config=*ink::BrushPackageCatalog{}.loadDefault(profile,4).package.compactConfig;
        content.stroke.config.startTaper=45;content.stroke.config.endTaper=70;content.stroke.knots.clear();
        for(unsigned i=0;i<2048;++i)content.stroke.knots.push_back({-350.+i*.8,140.+std::sin(i*.035)*65,static_cast<std::uint16_t>(14000+(i%300)*130)});
        entry.command=CompactStrokeReferenceCommand{content};entry.record.content=content;
        const auto prepared=geometry.get(entry,RenderCacheOptions{},geometryBudget);
        require(prepared&&prepared->isContour(),"GPU contour admission");
        for(unsigned pose:{3U,4U,5U,0U,1U,2U}){
            auto local=canvas->makeSurface(info),full=canvas->makeSurface(info);
            for(auto* surface:{local.get(),full.get()}){
                surface->getCanvas()->clear(SK_ColorBLACK);
                surface->getCanvas()->clipRect(SkRect::MakeLTRB(3,2,575,290));
                if(pose<3){surface->getCanvas()->translate(7.3F,-2.7F);
                    surface->getCanvas()->scale(pose==0?.8F:1.2F,pose==1?.9F:1.1F);
                    if(pose==2)surface->getCanvas()->rotate(11);
                }else{const auto scale=std::ldexp(1.F,int(pose)-4);
                    surface->getCanvas()->translate(2,2);surface->getCanvas()->scale(scale,scale);}
            }
            internal::PreparedGeometryDraw localWork;
            if(pose>=3)for(std::size_t first=0;first<prepared->primitiveCount();first+=32){
                const auto slice=prepared->drawContour(*local->getCanvas(),first,32);localWork.primitivesDrawn+=slice.primitivesDrawn;
            }else localWork=prepared->drawContour(*local->getCanvas());
            // Complete each target before starting the other. Even two full
            // reference streams differ at an AA boundary when queued together
            // on this shared Ganesh context. Readback is test-only, never a
            // production synchronization primitive or a latency measurement.
            std::vector<std::uint8_t> a(600*300*4),b(a.size());
            require(local->readPixels(info,a.data(),2400,0,0),"GPU prepared readback");
            internal::drawCompactContourCoverageToSkCanvas(*full->getCanvas(),content.stroke.config,
                ink::compileCompactContour(content.stroke.config,content.stroke.knots));
            require(full->readPixels(info,b.data(),2400,0,0),"GPU reference readback");
            require(a==b,"GPU local contour differs from full geometry");
            require(localWork.primitivesDrawn<prepared->primitiveCount(),"GPU contour locality missing");
        }
    }
    provider.release();
}
#endif
std::vector<std::uint8_t> checkLargeReplayNavigation(TestSurface& target,bool legacy=false){
    semantic::ObjectRecord object;object.id=ObjectId::fromUint64(88001);
    object.kind=semantic::ObjectKind::kVectorStroke;object.kind_version=3;
    semantic::CompactStrokeContent content;
    content.stroke.config=*ink::BrushPackageCatalog{}.loadDefault("round-handdrawn",4).package.compactConfig;
    content.stroke.seed=42;
    for(unsigned i=0;i<100000;++i)content.stroke.knots.push_back({i*.5,64.+std::sin(i*.01)*20,32768});
    object.content=content;
    const auto projected=projectRuntimeScene({&object,1},semantic::SemanticGeneration{1});
    ReferenceTraversalEntry entry{projected.records.front(),true,CompactStrokeReferenceCommand{content}};
    const auto frame=plan(8801,{entry});
    SkiaRenderer nav;RenderCacheOptions options;options.navigationMaxSteps=8;
    options.navigationMaxGpuContourPrimitives=32;options.navigationMicroseconds=1000000;
    if(legacy)options.maxEntryBytes=300000;
    nav.setRenderCacheOptions(options);
    NavigationSourceQuery query=[&](const foundation::WorldRect&,std::vector<ReferenceTraversalEntry>& out){out={entry};return true;};
    unsigned firstFeedback=0;
    for(unsigned tick=1;tick<=(legacy?4096U:12U);++tick){
        const auto gpu=nav.navigationWork().gpuContourPrimitives;
        require(navigate(nav,target,{1,-8,-4},frame.frame,query).code==BackendSubmissionCode::kAccepted,"large replay navigation rejected");
        require(nav.navigationWork().gpuContourPrimitives-gpu<=32,"large replay bypassed GPU primitive quota");
        if(nav.navigationWork().presented){firstFeedback=tick;break;}
    }
    require(firstFeedback>0,"offscreen v4 history starved complete feedback for12 owner opportunities");
    for(unsigned i=0;nav.navigationWork().pending;++i){
        require(i<(legacy?8192U:128U),"large replay refinement did not converge");
        require(navigate(nav,target,{1,-8,-4},frame.frame,query).code==BackendSubmissionCode::kAccepted,"large replay refinement rejected");
    }
    require(nav.submissionCount()==0,"navigation manufactured canonical submission");
    TestSurface reference;require(reference.resize(256,128).code==BackendSubmissionCode::kAccepted,"large replay reference");
    SkiaRenderer direct;RenderCacheOptions disabled;disabled.enabled=false;direct.setRenderCacheOptions(disabled);
    require(direct.renderFrame(reference,plan(8801,{entry},1,{1,0,0,1,-8,-4})).code==BackendSubmissionCode::kAccepted,"large replay Direct rejected");
    const auto feedback=pixels(target);
    // Tile feedback has its existing raster sampling; canonical retirement
    // still requires the exact frame, which is compared to Direct separately.
    require(nav.renderFrame(target,plan(8801,{entry},1,{1,0,0,1,-8,-4})).code==BackendSubmissionCode::kAccepted,"large replay canonical frame rejected");
    require(pixels(target)==pixels(reference),"large replay canonical frame changed exact pixels");
    std::cout<<"large navigation legacy="<<legacy<<" first feedback opportunity="<<firstFeedback<<'\n';
    return feedback;
}

void checkCoarseCameraStability(){
    RasterSkiaSurfaceProvider target;
    require(target.resize(1272,1846).code==BackendSubmissionCode::kAccepted,"portrait coarse surface");
    FrameState frame{ViewId{1},{},{0,0,636,923},{636,923,1272,1846,2,1},
        semantic::SemanticGeneration{8900},SceneRevision{8900},SurfaceGeneration{1},MetricsGeneration{1},FrameId{8900}};
    double prior=0;
    for(float x:{20.F,130.F}){
        SkiaRenderer nav;RenderCacheOptions options;options.navigationMaxQueries=1;options.navigationMaxSteps=1;options.navigationMicroseconds=1000000;nav.setRenderCacheOptions(options);
        double span=0;NavigationSourceQuery query=[&](const foundation::WorldRect& bounds,std::vector<ReferenceTraversalEntry>& out){span=bounds.right-bounds.left;out.clear();return true;};
        require(navigate(nav,target,{1.59F,-x*1.59F,-480*1.59F},frame,query).code==BackendSubmissionCode::kAccepted&&span>0,"portrait coarse query");
        if(prior)require(span==prior,"pan changed coarse level solely at a grid boundary");prior=span;
    }
}
void checkDenseFrontier(TestSurface& target,bool offscreen=false){
    std::vector<ReferenceTraversalEntry> entries;
    for(unsigned line=0;line<5;++line){
        auto entry=stroke(89000+line);auto c=std::get<CompactStrokeReferenceCommand>(entry.command).content.get();
        c.stroke.config=*ink::BrushPackageCatalog{}.loadDefault("round-handdrawn",4).package.compactConfig;
        c.stroke.knots.clear();
        for(unsigned i=0;i<2048;++i)c.stroke.knots.push_back({(offscreen?370.:110.)+std::sin(i*.017)*80.,64.+std::sin(i*.013+line)*42.,32768});
        entry.record.content=c;entry.command=CompactStrokeReferenceCommand{c};entries.push_back(entry);
    }
    auto crossing=stroke(89010);auto c=std::get<CompactStrokeReferenceCommand>(crossing.command).content.get();
    c.stroke.config=*ink::BrushPackageCatalog{}.loadDefault("round-handdrawn",4).package.compactConfig;
    c.stroke.knots={{230,40,32768},{290,80,48000}};crossing.record.content=c;crossing.command=CompactStrokeReferenceCommand{c};entries.push_back(crossing);
    const auto frame=plan(8901,entries);SkiaRenderer nav;
    RenderCacheOptions options;options.navigationMaxSteps=1;options.navigationMaxGpuContourPrimitives=32;options.navigationMicroseconds=1000000;
    nav.setRenderCacheOptions(options);
    for(unsigned i=0;;++i){
        const auto r=nav.renderFrame(target,frame);if(r.code==BackendSubmissionCode::kAccepted)break;
        require(i<256&&r.code==BackendSubmissionCode::kRetry,"dense canonical setup");
    }
    const auto canonical=pixels(target);
    NavigationSourceQuery query=[&](const foundation::WorldRect&,std::vector<ReferenceTraversalEntry>& out){out=entries;return true;};
    unsigned first=0;
    for(unsigned i=1;i<=8;++i){
        const auto gpu=nav.navigationWork().gpuContourPrimitives;
        require(navigate(nav,target,{1,-4,0},frame.frame,query).code==BackendSubmissionCode::kAccepted,"dense frontier navigation");
        require(nav.navigationWork().gpuContourPrimitives-gpu<=32,"frontier bypassed GPU quota");
        if(nav.navigationWork().presented){first=i;break;}
    }
    require(first>0,"tiny content frontier waited for unrelated dense cell interior");
    require(nav.navigationWork().coarseMaskStarts>0,"frontier fixture did not execute coarse CPU coverage");
    require(nav.navigationWork().alphaMaskStarts>0,"coarse frontier did not reuse qualified A8 coverage");
    require(nav.submissionCount()==1,"frontier forged canonical receipt");
    std::cout<<"dense frontier first feedback opportunity="<<first<<'\n';
    // Cold full-region rendering is an independent pixel oracle: it cannot
    // reuse partial frontier images or the retained canonical snapshot.
    for(const auto [dx,dy]:std::vector<std::pair<int,int>>{{4,0},{8,0},{16,4},{40,12}}){
        if(dx!=4)for(unsigned i=0;;++i){
            require(navigate(nav,target,{1,float(-dx),float(-dy)},frame.frame,query).code==BackendSubmissionCode::kAccepted,"frontier extension rejected");
            if(nav.navigationWork().presented)break;
            // The heavy variant brings its coils into view on the later pan;
            // unlike first feedback, that geometry must actually be drawn.
            require(i<(offscreen?4096U:32U),"frontier extension stalled");
        }
        TestSurface full;require(full.resize(256,128).code==BackendSubmissionCode::kAccepted,"frontier reference surface");
        SkiaRenderer reference;auto referenceOptions=options;referenceOptions.navigationMaxSteps=1;reference.setRenderCacheOptions(referenceOptions);
        for(unsigned i=0;;++i){
            require(navigate(reference,full,{1,float(-dx),float(-dy)},frame.frame,query).code==BackendSubmissionCode::kAccepted,"full-cell reference rejected");
            if(reference.navigationWork().presented)break;require(i<4096,"full-cell reference stalled");
        }
        const auto actual=pixels(target),expected=pixels(full);
        for(int y=0;y<128;++y)for(int x=0;x<256;++x)for(int channel=0;channel<4;++channel){
            const auto at=(y*256+x)*4+channel;
            if(x>=256-dx||y>=128-dy)
                {if(actual[at]!=expected[at])std::cerr<<"frontier mismatch dx="<<dx<<" dy="<<dy<<" x="<<x<<" y="<<y<<" c="<<channel<<" actual="<<int(actual[at])<<" expected="<<int(expected[at])<<"\n";
                require(actual[at]==expected[at],"partial/expanded cell changed frontier pixels or exposed unfinished content");}
            else require(actual[at]==canonical[((y+dy)*256+x+dx)*4+channel],"coarse frontier replaced sharp retained overlap");
        }
        require(nav.navigationWork().retainedBytes<=options.maxBytes,"partial cells exceeded residency budget");
    }
    const auto final=plan(8901,entries,1,{1,0,0,1,-40,-12});
    require(nav.renderFrame(target,final).code==BackendSubmissionCode::kAccepted,"frontier final canonical rejected");
    TestSurface exact;require(exact.resize(256,128).code==BackendSubmissionCode::kAccepted,"frontier exact reference surface");
    SkiaRenderer reference;RenderCacheOptions disabled;disabled.enabled=false;reference.setRenderCacheOptions(disabled);
    require(reference.renderFrame(exact,final).code==BackendSubmissionCode::kAccepted&&pixels(target)==pixels(exact),"frontier changed exact canonical pixels");
}

void checkFrontierRegionCompaction(TestSurface& target){
    std::vector<ReferenceTraversalEntry> entries;
    for(unsigned line=0;line<5;++line){
        auto entry=stroke(89200+line);auto c=std::get<CompactStrokeReferenceCommand>(entry.command).content.get();
        c.stroke.config=*ink::BrushPackageCatalog{}.loadDefault("round-handdrawn",4).package.compactConfig;c.stroke.config.smoothing=0;c.stroke.knots.clear();
        for(unsigned i=0;i<2048;++i)c.stroke.knots.push_back({490.+std::sin(i*.017)*8.,64.+std::sin(i*.013+line)*12.,32768});
        entry.record.content=c;entry.command=CompactStrokeReferenceCommand{c};entries.push_back(entry);
    }
    auto visible=stroke(89210);auto content=std::get<CompactStrokeReferenceCommand>(visible.command).content.get();
    content.stroke.config=*ink::BrushPackageCatalog{}.loadDefault("round-handdrawn",4).package.compactConfig;
    content.stroke.knots={{200,40,32768},{350,90,32768}};visible.record.content=content;visible.command=CompactStrokeReferenceCommand{content};entries.push_back(visible);
    auto corner=stroke(89211);auto mark=std::get<CompactStrokeReferenceCommand>(corner.command).content.get();
    mark.stroke.config=*ink::BrushPackageCatalog{}.loadDefault("round-handdrawn",4).package.compactConfig;
    mark.stroke.knots={{320,230,32768},{340,230,32768}};corner.record.content=mark;corner.command=CompactStrokeReferenceCommand{mark};entries.push_back(corner);
    const auto frame=plan(8920,entries);SkiaRenderer nav;RenderCacheOptions options;
    options.navigationMaxSteps=1;options.navigationMicroseconds=1000000;options.navigationMaxGpuContourPrimitives=32;nav.setRenderCacheOptions(options);
    for(unsigned i=0;;++i){auto r=nav.renderFrame(target,frame);if(r.code==BackendSubmissionCode::kAccepted)break;require(r.code==BackendSubmissionCode::kRetry&&i<256,"compaction canonical setup");}
    NavigationSourceQuery query=[&](const foundation::WorldRect&,std::vector<ReferenceTraversalEntry>& out){out=entries;return true;};
    for(unsigned step=1;step<=25;++step){
        for(unsigned i=0;;++i){
            require(navigate(nav,target,{1,float(-4*int(step)),float(2*int(step))},frame.frame,query).code==BackendSubmissionCode::kAccepted,"compaction navigation rejected");
            if(nav.navigationWork().presented)break;
            require(i<32,"small frontier compaction waited for unrelated full-cell ink");
        }
        const auto image=pixels(target);const auto sample=((56+2*step)*256+(248-4*step))*4;
        require(image[sample]<250,"compaction published a blank visible stroke");
        require(nav.navigationWork().retainedBytes<=options.maxBytes,"compaction escaped resource budget");
    }
    require(nav.navigationWork().regionCompactions>0,"compaction fixture never exhausted region metadata");
    // This zoom lies wholly inside the compacted rectangle but reaches a
    // previously unknown inside corner. A metadata-only union would be blank.
    require(navigate(nav,target,{4,-1136,-856},frame.frame,query).code==BackendSubmissionCode::kAccepted&&nav.navigationWork().presented,
        "compacted corner was not complete coverage");
    require(pixels(target)[(64*256+184)*4]<250,"compaction advertised an unpainted inside corner");
    const auto final=plan(8920,entries,1,{1,0,0,1,-100,50});
    require(nav.renderFrame(target,final).code==BackendSubmissionCode::kAccepted,"compaction final canonical");
    TestSurface full;require(full.resize(256,128).code==BackendSubmissionCode::kAccepted,"compaction reference surface");SkiaRenderer reference;
    RenderCacheOptions direct;direct.enabled=false;reference.setRenderCacheOptions(direct);
    require(reference.renderFrame(full,final).code==BackendSubmissionCode::kAccepted&&pixels(target)==pixels(full),"compaction changed final Direct pixels");
}

// Catch the oversized full-frame gate escaping cooperative preparation into
// synchronous Direct. The small target reproduces the DPR4 ratio without a
// slow performance threshold or a huge test allocation.
void checkCoverageQuotaDenial(TestSurface& target) {
    const auto acquired=target.acquire();require(acquired.code==SkiaSurfaceAcquireCode::kAcquired,"coverage denial target");
    auto budget=std::make_shared<resources::ResourceBudgetManager>(resources::BudgetLimits{64*1024,256*1024});
    const auto token=budget->reserve(resources::ResourceClass::kScratch,200*1024,true);
    require(bool(token),"coverage denial quota");resources::ResourceLease held(budget,*token);
    const auto config=*ink::BrushPackageCatalog{}.loadDefault("round-handdrawn",4).package.compactConfig;
    const std::vector<foundation::InkKnot> knots{{20,40,32768},{225,60,32768}};
    internal::CompactCoverageTargets targets;targets.budget=budget;
    internal::CompactStrokeRasterJob job(*acquired.frame.surface->getCanvas(),config,knots,nullptr,false,budget);
    bool escaped=false,complete=false;
    try{complete=job.step(*acquired.frame.surface->getCanvas(),config,knots,42,targets,false,nullptr);}
    catch(const std::bad_alloc&){escaped=true;}
    require(!escaped&&!complete,"coverage quota denial escaped as bad_alloc instead of yielding");
    held.reset();unsigned attempts=0;
    while(!complete){complete=job.step(*acquired.frame.surface->getCanvas(),config,knots,42,targets,false,nullptr);
        require(++attempts<64,"coverage quota release did not resume job");}
    target.release();
}
void checkOversizedCanonical(TestSurface& target) {
    checkCoverageQuotaDenial(target);
    TestSurface reference;require(reference.resize(256,128).code==BackendSubmissionCode::kAccepted,"oversized oracle surface");
    SkiaRenderer cached,direct;RenderCacheOptions options;options.maxBytes=64*1024;options.maxEntryBytes=16*1024;
    cached.setRenderCacheOptions(options);RenderCacheOptions disabled;disabled.enabled=false;direct.setRenderCacheOptions(disabled);
    auto entry=stroke(9821);auto content=std::get<CompactStrokeReferenceCommand>(entry.command).content.get();
    content.stroke.config=*ink::BrushPackageCatalog{}.loadDefault("round-handdrawn",4).package.compactConfig;
    content.stroke.knots.clear();
    for(unsigned i=0;i<160;++i)content.stroke.knots.push_back({20.+i*1.2,50.+std::sin(i*.1)*30,32768});
    entry.record.content=content;entry.command=CompactStrokeReferenceCommand{content};
    const auto demand=plan(9821,{entry},1,{1.1,0,0,1.1,-3.5,-2.5});
    const auto before=pixels(target);const auto presents=target.presentCount();
    RenderPreparationBudget slice;slice.maxCoverageSteps=1;slice.maxGroups=1;
    auto result=cached.renderFrame(target,demand,nullptr,&slice);
    require(result.code==BackendSubmissionCode::kRetry,"oversized canonical escaped to synchronous full-frame Direct");
    require(target.presentCount()==presents&&pixels(target)==before,"oversized preparation published partial pixels");
    require(cached.renderCacheWork().preparationCoverageSteps==1,"oversized preparation ignored one-step limit");
    unsigned attempts=0;
    while(result.code==BackendSubmissionCode::kRetry){
#if defined(G5_TEST_WEBGL)
        emscripten_sleep(1);
#endif
        result=cached.renderFrame(target,demand,nullptr,&slice);
        require(++attempts<1024,"oversized preparation did not converge");
    }
    require(result.code==BackendSubmissionCode::kAccepted&&target.presentCount()==presents+1,"oversized canonical receipt");
    require(direct.renderFrame(reference,demand).code==BackendSubmissionCode::kAccepted&&pixels(target)==pixels(reference),"oversized preparation changed Direct pixels");
    // A structurally impossible pair must fail explicitly, without touching
    // the presented target, retrying forever, or accepting a Direct frame.
    SkiaRenderer impossible;options.sharedMaxBytes=192*1024;impossible.setRenderCacheOptions(options);
    const auto stable=pixels(target);const auto count=target.presentCount();
    require(impossible.renderFrame(target,demand,nullptr,&slice).code==BackendSubmissionCode::kRejected&&
        pixels(target)==stable&&target.presentCount()==count,"impossible work buffers escaped admission");
    // Non-isolated transformed ink writes into the composite directly. It
    // still needs a coverage tile even though it does not need full scratch.
    auto transformed=entry;transformed.record.transform.tx=2;
    SkiaRenderer compositeOnly;options.sharedMaxBytes=128*1024;compositeOnly.setRenderCacheOptions(options);
    require(compositeOnly.renderFrame(target,plan(9822,{transformed}),nullptr,&slice).code==BackendSubmissionCode::kRejected&&
        pixels(target)==stable&&target.presentCount()==count,"composite-only coverage cannot fit but retries forever");

    // Inject real shared quota pressure, then retire it. No allocator mock:
    // the production cache must keep offscreen progress and resume admission.
    internal::SkiaRenderCache cache;options.sharedMaxBytes=600*1024;cache.options=options;
    cache.budget=std::make_shared<resources::ResourceBudgetManager>(resources::BudgetLimits{64*1024,600*1024});
    const auto token=cache.budget->reserve(resources::ResourceClass::kScratch,400*1024,true);
    require(bool(token),"quota pressure setup");resources::ResourceLease held(cache.budget,*token);
    const auto acquired=target.acquire();require(acquired.code==SkiaSurfaceAcquireCode::kAcquired,"quota target");
    cache.begin(demand,&target,*acquired.frame.surface,0);
    auto preparation=cache.prepare(*acquired.frame.surface->getCanvas(),demand,slice,nullptr,nullptr);
    require(preparation==internal::SkiaRenderCache::Preparation::kResourcePending&&
        cache.work.preparationCoverageSteps==0,"denied scratch escaped into synchronous work");
    require(!cache.compositePreparedFrame(*acquired.frame.surface->getCanvas()),"denied scratch qualified incomplete pixels");
    held.reset();unsigned resumed=0;
    do{preparation=cache.prepare(*acquired.frame.surface->getCanvas(),demand,slice,nullptr,nullptr);
        require(++resumed<1024,"retired quota did not resume work");}
    while(preparation==internal::SkiaRenderCache::Preparation::kPending);
    require(preparation==internal::SkiaRenderCache::Preparation::kReady&&
        cache.compositePreparedFrame(*acquired.frame.surface->getCanvas()),"retired quota failed canonical preparation");
    target.release();require(pixels(target)==pixels(reference),"quota recovery changed exact pixels");
}

// Resident images use one tile allocation after completion; only active
// preparation needs the extra two buffers. A cold viewport must not remain
// blurry solely because planning charges every finished image at peak size.
void checkResidentNavigationDensity(){
    TestSurface target,reference;
    require(target.resize(1024,768).code==BackendSubmissionCode::kAccepted&&reference.resize(1024,768).code==BackendSubmissionCode::kAccepted,"resident density targets");
    semantic::ObjectRecord object;object.id=ObjectId::fromUint64(9870);object.kind=semantic::ObjectKind::kShape;object.kind_version=1;
    object.content=semantic::ShapeContent{1,192,120};object.transform.tx=32;object.transform.ty=40;
    const auto projected=projectRuntimeScene({&object,1},semantic::SemanticGeneration{1});
    const ReferenceTraversalEntry entry{projected.records.front(),true,ShapeReferenceCommand{std::get<semantic::ShapeContent>(object.content)}};
    FrameState frame{ViewId{1},CameraState{{0,0},1,0,CameraGeneration{1}},{0,0,512,384},
        {512,384,1024,768,2,2},semantic::SemanticGeneration{1},SceneRevision{1},SurfaceGeneration{1},MetricsGeneration{1},FrameId{9870}};
    ReferenceDrawList list{frame};list.viewportClip={0,0,512,384};list.entries={entry};FramePlan canonical{frame,std::move(list)};
    SkiaRenderer cached,direct;RenderCacheOptions options;options.maxBytes=6*1024*1024;
    options.navigationMicroseconds=1000000;cached.setRenderCacheOptions(options);
    RenderCacheOptions disabled;disabled.enabled=false;direct.setRenderCacheOptions(disabled);
    NavigationSourceQuery query=[&](const foundation::WorldRect&,std::vector<ReferenceTraversalEntry>& out){out={entry};return true;};
    unsigned ticks=0;
    do{require(navigate(cached,target,{1,0,0},frame,query,false).code==BackendSubmissionCode::kAccepted,"resident density navigation");
        require(cached.navigationWork().retainedBytes<=options.maxBytes,"resident density escaped tile budget");
        require(++ticks<256,"resident density preparation stalled");
    }while(cached.navigationWork().pending);
    require(cached.navigationWork().maximumUpscale<=1.25&&cached.navigationWork().lowResolutionFraction==0,
        "resident tile capacity left avoidable low-resolution debt");
    require(cached.submissionCount()==0,"navigation density emitted canonical submission");
    require(direct.renderFrame(reference,canonical).code==BackendSubmissionCode::kAccepted,"resident density Direct reference");
    const auto read=[](TestSurface& surface){const auto a=surface.acquire();require(a.code==SkiaSurfaceAcquireCode::kAcquired,"resident density read");
        const auto info=a.frame.surface->imageInfo();std::vector<std::uint8_t> bytes(info.computeMinByteSize());
        require(a.frame.surface->readPixels(info,bytes.data(),info.minRowBytes(),0,0),"resident density readback");surface.release();return bytes;};
    require(read(target)==read(reference),"resident density pixels differ from Direct");
}

void checkObsoleteExactNavigation(){
    TestSurface target,reference;require(target.resize(2048,1024).code==BackendSubmissionCode::kAccepted&&reference.resize(2048,1024).code==BackendSubmissionCode::kAccepted,"obsolete exact targets");
    const auto make=[](std::uint64_t scene,std::vector<ReferenceTraversalEntry> entries,WorldToViewAffine transform=WorldToViewAffine{}){
        FrameState frame{ViewId{1},CameraState{{0,0},1,0,CameraGeneration{1}},{0,0,512,256},
            {512,256,2048,1024,4,4},semantic::SemanticGeneration{scene},SceneRevision{scene},SurfaceGeneration{1},MetricsGeneration{1},FrameId{scene}};
        ReferenceDrawList list{frame};list.viewportClip={0,0,512,256};list.entries=std::move(entries);list.worldToView=transform;
        return FramePlan{frame,std::move(list)};
    };
    auto first=stroke(9871);auto second=stroke(9872,70);
    for(auto* entry:{&first,&second}){auto content=std::get<CompactStrokeReferenceCommand>(entry->command).content.get();
        content.stroke.config=*ink::BrushPackageCatalog{}.loadDefault("round-handdrawn",4).package.compactConfig;
        content.stroke.knots.clear();for(unsigned i=0;i<160;++i)content.stroke.knots.push_back({10.+i*3.,120.+std::sin(i*.1)*100,32768});
        semantic::ObjectRecord object;object.id=entry->record.objectId;object.kind=semantic::ObjectKind::kVectorStroke;
        object.kind_version=3;object.content=content;
        entry->record=projectRuntimeScene({&object,1},semantic::SemanticGeneration{1}).records.front();entry->command=CompactStrokeReferenceCommand{content};}
    SkiaRenderer cached,direct;RenderCacheOptions options;options.maxBytes=4*1024*1024;options.maxEntryBytes=512*1024;
    options.sharedMaxBytes=24*1024*1024+768*1024;options.navigationMicroseconds=1000000;cached.setRenderCacheOptions(options);
    RenderCacheOptions disabled;disabled.enabled=false;direct.setRenderCacheOptions(disabled);
    RenderPreparationBudget slice;slice.maxCoverageSteps=1;slice.maxGroups=1;
    const auto warm=make(9871,{first});BackendSubmissionResult result;unsigned ticks=0;
    do{
#if defined(G5_TEST_WEBGL)
        emscripten_sleep(1);
#endif
        result=cached.renderFrame(target,warm,nullptr,&slice);require(++ticks<1024,"obsolete exact warmup stalled");
    }while(result.code==BackendSubmissionCode::kRetry);
    require(result.code==BackendSubmissionCode::kAccepted,"obsolete exact warmup");
    const auto pending=make(9872,{first,second});
    require(cached.renderFrame(target,pending,nullptr,&slice).code==BackendSubmissionCode::kRetry,"obsolete exact setup did not suspend");
    const auto steps=cached.renderCacheWork().preparationCoverageSteps,allocations=cached.renderCacheWork().scratchAllocations;
    NavigationSourceQuery query=[&](const foundation::WorldRect&,std::vector<ReferenceTraversalEntry>& out){out={first,second};return true;};
    // A matching demand must not throw away its cursor/checkpoints.
    (void)navigate(cached,target,{1,0,0},pending.frame,query,false);
    require(cached.renderFrame(target,pending,nullptr,&slice).code==BackendSubmissionCode::kRetry&&
        cached.renderCacheWork().preparationCoverageSteps==steps+1&&cached.renderCacheWork().scratchAllocations==allocations,"matching navigation discarded exact progress");
    const auto count=cached.submissionCount(),presents=target.presentCount();bool feedback=false;
    for(unsigned i=0;i<512&&!feedback;++i){
        const auto nav=navigate(cached,target,{1.3F,-280,-140},pending.frame,query,false);
        require(nav.code==BackendSubmissionCode::kAccepted,nav.message.c_str());
        feedback=cached.navigationWork().presented;
    }
    require(feedback&&target.presentCount()>presents,"obsolete exact buffers starved renewed navigation");
    require(cached.submissionCount()==count,"obsolete exact cancellation qualified canonical work");
    const auto current=make(9872,{first,second},{1.3F,0,0,1.3F,-280,-140});ticks=0;
    do{
#if defined(G5_TEST_WEBGL)
        emscripten_sleep(1);
#endif
        result=cached.renderFrame(target,current,nullptr,&slice);require(++ticks<1024,"new-camera exact recovery stalled");
    }while(result.code==BackendSubmissionCode::kRetry);
    require(result.code==BackendSubmissionCode::kAccepted,"new-camera exact recovery");
    require(direct.renderFrame(reference,current).code==BackendSubmissionCode::kAccepted,"new-camera Direct oracle");
    const auto read=[](TestSurface& surface){const auto a=surface.acquire();require(a.code==SkiaSurfaceAcquireCode::kAcquired,"obsolete exact read");
        const auto info=a.frame.surface->imageInfo();std::vector<std::uint8_t> bytes(info.computeMinByteSize());
        require(a.frame.surface->readPixels(info,bytes.data(),info.minRowBytes(),0,0),"obsolete exact readback");surface.release();return bytes;};
    require(read(target)==read(reference),"obsolete exact recovery changed canonical pixels");
}

// A qualified zoom-in frame must not discard the wider same-scene coverage
// needed by the next gesture. One bounded tile step cannot rebuild that plane.
void checkRetainedZoomReentry(){
    TestSurface target;require(target.resize(256,128).code==BackendSubmissionCode::kAccepted,"reentry target");
    SkiaRenderer renderer;RenderCacheOptions options;options.navigationMaxSteps=1;options.navigationMicroseconds=1000000;
    renderer.setRenderCacheOptions(options);const auto entry=stroke(9881);
    const auto wide=plan(9881,{entry});
    require(renderer.renderFrame(target,wide).code==BackendSubmissionCode::kAccepted,"reentry wide source");
    const auto widePixels=pixels(target);
    const auto narrow=plan(9881,{entry},1,{2,0,0,2,-128,0});
    require(renderer.renderFrame(target,narrow).code==BackendSubmissionCode::kAccepted,"reentry zoomed canonical");
    const auto canonical=renderer.submissionCount(),presents=target.presentCount();
    NavigationSourceQuery query=[&](const foundation::WorldRect&,std::vector<ReferenceTraversalEntry>& out){out={entry};return true;};
    (void)navigate(renderer,target,{1,0,0},wide.frame,query,false);
    require(target.presentCount()==presents+1&&renderer.navigationWork().presented,
        "zoom-in canonical discarded wide coverage before resumed zoom-out");
    require(renderer.submissionCount()==canonical,"reentry feedback emitted canonical receipt");
    const auto resumed=pixels(target);const auto point=(42*256+24)*4;
    require(resumed[point]==widePixels[point]&&resumed[point]<180,"reentry lost content outside zoom-in source");
    const auto outside=target.presentCount();
    (void)navigate(renderer,target,{1,512,0},wide.frame,query,false);
    require(target.presentCount()==outside,"reentry fallback certified unknown outside both images");
    const auto before=target.presentCount();const auto changed=plan(9882,{entry});
    (void)navigate(renderer,target,{1,0,0},changed.frame,query,false);
    require(target.presentCount()==before,"reentry retained source crossed scene identity");
}

void checkSharedCanonicalNavigation(TestSurface&) {
    TestSurface target,reference;
    require(target.resize(2048,1024).code==BackendSubmissionCode::kAccepted&&reference.resize(2048,1024).code==BackendSubmissionCode::kAccepted,"shared DPR4 targets");
    const auto makePlan=[](std::uint64_t id,std::vector<ReferenceTraversalEntry> entries,WorldToViewAffine transform=WorldToViewAffine{}){
        FrameState f{ViewId{1},CameraState{{0,0},1,0,CameraGeneration{1}},{0,0,512,256},
            {512,256,2048,1024,4,4},semantic::SemanticGeneration{id},SceneRevision{id},SurfaceGeneration{1},MetricsGeneration{1},FrameId{id}};
        ReferenceDrawList list{f};list.viewportClip={0,0,512,256};list.worldToView=transform;list.entries=std::move(entries);
        return FramePlan{f,std::move(list)};
    };
    const auto read=[](TestSurface& surface){
        const auto a=surface.acquire();require(a.code==SkiaSurfaceAcquireCode::kAcquired,"shared frame readback acquire");
        const auto info=a.frame.surface->imageInfo();std::vector<std::uint8_t> bytes(info.computeMinByteSize());
        require(a.frame.surface->readPixels(info,bytes.data(),info.minRowBytes(),0,0),"shared frame readback");surface.release();return bytes;
    };
    // The required fine/coarse tile set fits,but the8MiB frame cannot be
    // duplicated into the4MiB optional cache:the real DPR4 gate relationship.
    SkiaRenderer cached,direct;RenderCacheOptions options;options.maxBytes=4*1024*1024;options.maxEntryBytes=16*1024;
    options.navigationMaxSteps=1;options.navigationMaxQueries=4;
    cached.setRenderCacheOptions(options);RenderCacheOptions disabled;disabled.enabled=false;direct.setRenderCacheOptions(disabled);
    const auto entry=stroke(9861);const auto frame=makePlan(9861,{entry});RenderPreparationBudget slice;slice.maxCoverageSteps=1;
    unsigned attempts=0;BackendSubmissionResult result;
    do{result=cached.renderFrame(target,frame,nullptr,&slice);require(++attempts<128,"shared frame setup convergence");}
    while(result.code==BackendSubmissionCode::kRetry);
    require(result.code==BackendSubmissionCode::kAccepted,"shared canonical frame setup");
    require(cached.renderCacheWork().preparationBudgetEntries>0,"share fixture did not create a streamed composite");
    NavigationSourceQuery query=[&](const foundation::WorldRect&,std::vector<ReferenceTraversalEntry>& out){out={entry};return true;};
    NavigationBackgroundQuery empty=[](const foundation::WorldRect&){return true;};
    const auto presents=target.presentCount();
    require(navigate(cached,target,{1,4,0},frame.frame,query,false,nullptr,empty).code==BackendSubmissionCode::kAccepted&&
        target.presentCount()==presents+1,"completed canonical pixels were lost at optional snapshot capacity");
    require(direct.renderFrame(reference,makePlan(9862,{entry},{1,0,0,1,4,0})).code==BackendSubmissionCode::kAccepted&&
        read(target)==read(reference),"shared sharp overlap changed Direct pixels");
    const auto count=target.presentCount();const auto changed=makePlan(9863,{entry});
    (void)navigate(cached,target,{1,4,0},changed.frame,query,false,nullptr,empty);
    require(target.presentCount()==count,"shared frame crossed semantic/scene binding");
}
void checkSharedFrameAccounting(TestSurface& target){
    for(bool sampled:{false,true}){
        auto budget=std::make_shared<resources::ResourceBudgetManager>(resources::BudgetLimits{64*1024,8*1024*1024});
        internal::SkiaRenderCache cache;cache.budget=budget;cache.options.maxBytes=64*1024;cache.options.maxEntryBytes=16*1024;
        const auto frame=plan(9864,{stroke(9864)});RenderPreparationBudget slice;slice.maxCoverageSteps=1;
        const auto write=budget->beginGpuEpoch(internal::GpuResourceRetirement::canonicalAllocations,
            internal::GpuResourceRetirement::canonicalReads);require(bool(write),"shared frame write epoch");
        const auto a=target.acquire();require(a.code==SkiaSurfaceAcquireCode::kAcquired,"shared frame accounting acquire");
        cache.begin(frame,&target,*a.frame.surface,0);internal::SkiaRenderCache::Preparation result;
        unsigned attempts=0;
        do{result=cache.prepare(*a.frame.surface->getCanvas(),frame,slice,nullptr,nullptr);require(++attempts<128,"shared accounting convergence");}
        while(result==internal::SkiaRenderCache::Preparation::kPending);
        require(result==internal::SkiaRenderCache::Preparation::kReady&&cache.compositePreparedFrame(*a.frame.surface->getCanvas()),"shared accounting completion");
        target.release();(void)pixels(target); // Test-only real GPU completion before retiring the write epoch.
        require(budget->endGpuEpoch(*write)&&budget->completeGpuEpoch(*write),"shared write retirement");
        auto first=cache.completedFrame(),alias=first;require(bool(first),"completed immutable image");
        cache.clear();first.reset();
        require(budget->used(resources::ResourceClass::kCanonicalFrame)==128*1024,"aliases duplicated or lost the one pixel charge");
        const auto epoch=budget->beginGpuEpoch(internal::GpuResourceRetirement::canonicalAllocations,
            internal::GpuResourceRetirement::canonicalReads);require(bool(epoch),"shared read epoch");
        if(sampled){alias->markGpuUse();const auto draw=target.acquire();require(draw.code==SkiaSurfaceAcquireCode::kAcquired,"shared sample acquire");
            draw.frame.surface->getCanvas()->drawImage(alias->image,0,0);target.release();}
        alias.reset();require(budget->used(resources::ResourceClass::kCanonicalFrame)==(sampled?128*1024:0),
            "shared frame lifetime did not distinguish actual reads from unused retention");
        require(budget->endGpuEpoch(*epoch),"shared read scope end");
        if(sampled)(void)pixels(target);
        require(budget->completeGpuEpoch(*epoch)&&budget->used()==0,"shared frame read retirement leaked");
    }
}
template<class Job>
std::unique_ptr<Job> canonicalReplayJob(SkCanvas& canvas,const foundation::InkBrushConfig& config,
    std::span<const foundation::InkKnot> knots,std::shared_ptr<resources::ResourceBudgetManager> budget){
    using Reclaim=std::function<void(std::size_t)>;
    // The frozen RED oracle remains runnable against the old constructor.
    if constexpr(std::is_constructible_v<Job,SkCanvas&,const foundation::InkBrushConfig&,
        std::span<const foundation::InkKnot>,const internal::CompactDeviceGrid*,bool,
        std::shared_ptr<resources::ResourceBudgetManager>,std::size_t,Reclaim,bool>)
        return std::make_unique<Job>(canvas,config,knots,nullptr,true,budget,8*1024*1024,Reclaim{},true);
    else return std::make_unique<Job>(canvas,config,knots,nullptr,true,budget);
}
void checkCanonicalReplayHeadroom(TestSurface& provider){
    const auto acquired=provider.acquire();require(acquired.code==SkiaSurfaceAcquireCode::kAcquired,"critical replay root");
    const auto info=acquired.frame.surface->imageInfo().makeWH(512,384);
    auto actual=acquired.frame.surface->makeSurface(info),oracle=acquired.frame.surface->makeSurface(info);
    require(actual&&oracle,"critical replay targets");actual->getCanvas()->clear(SK_ColorTRANSPARENT);oracle->getCanvas()->clear(SK_ColorTRANSPARENT);
    auto config=*ink::BrushPackageCatalog{}.loadDefault("round-handdrawn",4).package.compactConfig;
    std::vector<foundation::InkKnot> knots;for(unsigned i=0;i<1024;++i)knots.push_back({5.+i*.49,190.+std::sin(i*.04)*160,32768});
    auto budget=std::make_shared<resources::ResourceBudgetManager>(resources::BudgetLimits{64*1024,16*1024*1024});
    const auto token=budget->reserve(resources::ResourceClass::kScratch,2*1024*1024,true);require(bool(token),"critical replay occupied soft quota");
    resources::ResourceLease occupied(budget,*token);internal::CompactCoverageTargets targets;targets.budget=budget;
    internal::CompactRasterWork work;
    {
        auto job=canonicalReplayJob<internal::CompactStrokeRasterJob>(*actual->getCanvas(),config,knots,budget);
        for(unsigned i=0;!job->step(*actual->getCanvas(),config,knots,42,targets,true,&work);++i)
            require(i<10000,"critical replay convergence");
        require(work.compiledContourKnots==knots.size()&&work.contourRecordReplays>0,
            "canonical replay recompiled contour despite bounded hard headroom");
        require(budget->used(resources::ResourceClass::kRecorded)<=8*1024*1024&&budget->used()<=16*1024*1024,"critical replay exceeded admission caps");
    }
    std::vector<std::uint8_t> got(info.computeMinByteSize()),want(got.size());
    require(actual->readPixels(info,got.data(),info.minRowBytes(),0,0),"critical replay readback");
    internal::drawCompactStrokeToSkCanvas(*oracle->getCanvas(),config,knots,42);
    require(oracle->readPixels(info,want.data(),info.minRowBytes(),0,0)&&got==want,"critical replay changed pinned Direct pixels");
    targets.clear();occupied.reset();require(budget->used()==0,"critical replay lease leaked");provider.release();
    // Denial part-way through an admitted canonical stream must preserve the
    // complete checkpoint index and exact fallback,without reclaiming navigation.
    const auto again=provider.acquire();require(again.code==SkiaSurfaceAcquireCode::kAcquired,"critical fallback root");
    auto denied=again.frame.surface->makeSurface(info);require(bool(denied),"critical fallback target");denied->getCanvas()->clear(SK_ColorTRANSPARENT);
    auto limited=std::make_shared<resources::ResourceBudgetManager>(resources::BudgetLimits{64*1024,4*1024*1024});
    internal::CompactCoverageTargets limitedTargets;limitedTargets.budget=limited;internal::CompactRasterWork fallback;
    {
        auto job=canonicalReplayJob<internal::CompactStrokeRasterJob>(*denied->getCanvas(),config,knots,limited);
        require(!job->step(*denied->getCanvas(),config,knots,42,limitedTargets,true,&fallback)&&
            limited->used(resources::ResourceClass::kRecorded)>0,"canonical partial denial did not admit first record");
        const auto pressure=limited->reserve(resources::ResourceClass::kScratch,4*1024*1024-limited->used()-132*1024,true);
        require(bool(pressure),"canonical partial hard pressure");resources::ResourceLease pressureLease(limited,*pressure);
        for(unsigned i=0;!job->step(*denied->getCanvas(),config,knots,42,limitedTargets,true,&fallback);++i)
            require(i<10000&&!job->resourcePending(),"canonical partial denial lost bounded checkpoint fallback");
        require(fallback.contourRecordReplays==0&&fallback.checkpointReplayKnots>0,"canonical hard denial did not use exact checkpoints");
    }
    std::vector<std::uint8_t> fallbackPixels(want.size());
    require(denied->readPixels(info,fallbackPixels.data(),info.minRowBytes(),0,0)&&fallbackPixels==want,"canonical hard denial changed Direct pixels");
    limitedTargets.clear();require(limited->used()==0,"canonical hard fallback leaked quota");provider.release();
}

#if defined(G5_TEST_WEBGL)
// Keep real GPU work pending without blocking or reporting false completion.
struct DelayedRetirementPoll final {
    DelayedRetirementPoll(){EM_ASM({const p=WebGL2RenderingContext.prototype;
        window.retirementOriginalPoll=p.clientWaitSync;p.clientWaitSync=function(...args){
            const r=window.retirementOriginalPoll.apply(this,args);return r===this.WAIT_FAILED?r:this.TIMEOUT_EXPIRED;};});}
    ~DelayedRetirementPoll(){restore();}
    void restore(){if(active){EM_ASM({WebGL2RenderingContext.prototype.clientWaitSync=window.retirementOriginalPoll;});active=false;}}
    bool active=true;
};
std::vector<std::uint8_t> retirementPixels(TestSurface& provider){
    const auto a=provider.acquire();require(a.code==SkiaSurfaceAcquireCode::kAcquired,"retirement read acquire");
    const auto info=a.frame.surface->imageInfo();std::vector<std::uint8_t> out(info.computeMinByteSize());
    require(a.frame.surface->readPixels(info,out.data(),info.minRowBytes(),0,0),"retirement full readback");provider.release();return out;
}
void checkRetirementFrame(){
    TestSurface target,reference;require(target.resize(1024,768).code==BackendSubmissionCode::kAccepted&&
        reference.resize(1024,768).code==BackendSubmissionCode::kAccepted,"retirement frame targets");
    FrameState state{ViewId{1},CameraState{{0,0},1,0,CameraGeneration{1}},{0,0,512,384},
        {512,384,1024,768,2,2},semantic::SemanticGeneration{9871},SceneRevision{9871},SurfaceGeneration{1},MetricsGeneration{1},FrameId{9871}};
    ReferenceDrawList list{state};list.viewportClip={0,0,512,384};list.entries={stroke(9871)};
    const FramePlan frame{state,std::move(list)};
    SkiaRenderer cached,direct;RenderCacheOptions options;options.maxBytes=4*1024*1024;
    options.maxEntryBytes=16*1024;options.sharedMaxBytes=8*1024*1024;cached.setRenderCacheOptions(options);
    RenderCacheOptions off;off.enabled=false;direct.setRenderCacheOptions(off);
    RenderPreparationBudget slice;slice.maxCoverageSteps=64;slice.maxMicroseconds=1000000;
    BackendSubmissionResult warm;
    for(unsigned i=0;;++i){warm=cached.renderFrame(target,frame,nullptr,&slice);if(warm.code!=BackendSubmissionCode::kRetry)break;
        require(i<128,"retirement warm frame convergence");emscripten_sleep(1);}
    require(warm.code==BackendSubmissionCode::kAccepted&&cached.renderCacheWork().scratchPeakBytes==3*1024*1024,"retirement frame did not use stable full scratch");
    const auto sharp=retirementPixels(target);const auto canonical=cached.submissionCount();
    DelayedRetirementPoll delayed;
    unsigned queries=0;NavigationSourceQuery unavailable=[&](const foundation::WorldRect&,std::vector<ReferenceTraversalEntry>&){++queries;return false;};
    require(navigate(cached,target,{1,0,0},state,unavailable,false).code==BackendSubmissionCode::kAccepted,
        "retirement old feedback setup");
    auto movingList=frame.referenceDrawList;movingList.worldToView={1.2,0,0,1.2,-12,-6};const FramePlan moving{state,std::move(movingList)};
    const auto presents=target.presentCount();
    const auto pending=cached.renderFrame(target,moving,nullptr,&slice);
    require(pending.code==BackendSubmissionCode::kRetry&&target.presentCount()==presents&&cached.submissionCount()==canonical,
        "retirement pressure published partial canonical work");
    const auto feedback=cached.renderNavigation(target,{1,0,0},state);
    delayed.restore();
    require(feedback.code==BackendSubmissionCode::kAccepted&&queries==0&&retirementPixels(target)==sharp,
        "already-retiring scratch caused qualified whole-frame feedback eviction");
    require(cached.submissionCount()==canonical,"retirement feedback manufactured canonical receipt");
    BackendSubmissionResult resumed;
    for(unsigned i=0;;++i){emscripten_sleep(1);resumed=cached.renderFrame(target,moving,nullptr,&slice);
        if(resumed.code!=BackendSubmissionCode::kRetry)break;require(i<256,"retirement frame resume stalled");}
    require(resumed.code==BackendSubmissionCode::kAccepted&&direct.renderFrame(reference,moving).code==BackendSubmissionCode::kAccepted&&
        retirementPixels(target)==retirementPixels(reference),"retirement resume changed Direct canonical pixels");
    std::cout<<"{\"retirement_frame\":\"PASS\",\"canonical_before\":"<<canonical<<",\"canonical_after\":"<<cached.submissionCount()<<"}\n";
}
void checkRetirementCells(){
    constexpr std::size_t mib=1024*1024,hard=12*mib;
    for(bool retiring:{true,false}){
        TestSurface target;require(target.resize(256,128).code==BackendSubmissionCode::kAccepted,"retirement cell target");
        auto nav=std::make_unique<internal::SkiaNavigationTiles>();auto retirement=std::make_shared<internal::GpuResourceRetirement>();
        auto budget=std::make_shared<resources::ResourceBudgetManager>(resources::BudgetLimits{4*mib,hard});
        RenderCacheOptions options;options.maxBytes=4*mib;options.maxEntryBytes=4*mib;options.sharedMaxBytes=hard;
        options.navigationMaxSteps=64;options.navigationMicroseconds=1000000;
        auto entry=stroke(9872);auto content=std::get<CompactStrokeReferenceCommand>(entry.command).content.get();
        content.stroke.config=*ink::BrushPackageCatalog{}.loadDefault("round-handdrawn",4).package.compactConfig;
        entry.record.content=content;entry.command=CompactStrokeReferenceCommand{content};const auto frame=plan(9872,{entry});
        NavigationSourceQuery query=[&](const foundation::WorldRect&,std::vector<ReferenceTraversalEntry>& out){out={entry};return true;};
        for(unsigned i=0;;++i){emscripten_sleep(1);
            require(nav->render(target,{1,0,0},frame.frame,query,1,options,budget,false,nullptr,nullptr,nullptr,{}, {},retirement).code==BackendSubmissionCode::kAccepted,"retirement cell warm");
            if(!nav->work.pending)break;require(i<256,"retirement cells setup stalled");}
        (void)retirementPixels(target);
        for(unsigned i=0;budget->pendingGpuEpochCount();++i){emscripten_sleep(1);retirement->service();require(i<128,"retirement warm drain");}
        const auto cells=budget->used(resources::ResourceClass::kNavigationTile);
        const auto evictions=nav->work.evictions;
        require(cells>128*1024,"retirement cells fixture empty");
        DelayedRetirementPoll delayed;resources::ResourceLease pressureLease;sk_sp<SkSurface> pressure;
        const auto a=target.acquire();require(a.code==SkiaSurfaceAcquireCode::kAcquired,"retirement pressure acquire");
        {
            internal::SkiaGpuLifetime gpu(*retirement,budget,target,GrAsDirectContext(a.frame.surface->recordingContext()),
                resources::resourceClassMask(resources::ResourceClass::kCanonicalFrame),0);
            require(gpu.ready(),"retirement pressure GPU scope");
            const auto token=budget->reserve(resources::ResourceClass::kCanonicalFrame,8*mib,true);require(bool(token),"retirement pressure lease");
            pressureLease=resources::ResourceLease(budget,*token);pressure=a.frame.surface->makeSurface(SkImageInfo::MakeN32Premul(2048,1024));
            require(bool(pressure),"retirement pressure physical surface");pressure->getCanvas()->clear(SK_ColorRED);
            a.frame.surface->getCanvas()->drawImage(pressure->makeImageSnapshot(),0,0);gpu.touch();
            if(retiring){pressure.reset();pressureLease.reset();}
        }
        target.release();
        const auto used=budget->used(),requested=hard-used+std::min<std::size_t>(64*1024,cells/2);
        require(!budget->reserve(resources::ResourceClass::kScratch,requested,true),"retirement pressure did not deny actual admission");
        const auto pending=budget->snapshot(resources::ResourceClass::kCanonicalFrame).pendingRelease;
        require((retiring&&pending==8*mib)||(!retiring&&pending==0),"retirement fixture did not distinguish live from pending frame bytes");
        nav->reclaimForExact(budget,hard,requested,frame.frame,&target,1,true);
        const auto evicted=nav->work.evictions-evictions;
        delayed.restore();
        if(retiring){
            require(evicted==0&&budget->used(resources::ResourceClass::kNavigationTile)==cells,
                "already-retiring frame caused unnecessary navigation cell eviction");
            require(!budget->reserve(resources::ResourceClass::kScratch,requested,true),"future quota was uncharged before real completion");
        }else require(evicted>0,"mandatory live pressure failed to reclaim cells");
        (void)retirementPixels(target);
        for(unsigned i=0;budget->pendingGpuEpochCount();++i){emscripten_sleep(1);retirement->service();require(i<128,"retirement pressure drain");}
        const auto admitted=budget->reserve(resources::ResourceClass::kScratch,requested,true);require(bool(admitted),"retirement quota did not resume");budget->release(*admitted);
        nav.reset();pressure.reset();pressureLease.reset();retirement->service();require(budget->used()==0,"retirement owner leaked quota");
        std::cout<<"{\"retirement_cells\":\"PASS\",\"retiring\":"<<retiring<<",\"evictions\":"<<evicted<<"}\n";
    }
}
#endif

void checkErasedInkOverShapeNavigation() {
    for(bool filled:{false,true})for(unsigned revision:{2U,4U}){
        semantic::ObjectRecord object;object.id=ObjectId::fromUint64(99001);
        object.kind=semantic::ObjectKind::kShape;object.kind_version=1;
        object.content=semantic::ShapeContent{0,256,128};
        object.properties.entries.push_back({0x100U,semantic::FillStyleValue{semantic::SolidFill{{0,0,1,1}}}});
        const auto projection=projectRuntimeScene({&object,1},semantic::SemanticGeneration{1});
        ReferenceTraversalEntry blue{projection.records.front(),true,ShapeReferenceCommand{std::get<semantic::ShapeContent>(object.content)}};
        auto erased=stroke(99002);
        auto content=std::get<CompactStrokeReferenceCommand>(erased.command).content.get();
        content.stroke.config=*ink::BrushPackageCatalog{}.loadDefault("round-handdrawn",revision).package.compactConfig;
        erased.record.content=content;erased.command=CompactStrokeReferenceCommand{content};
        semantic::EraseMaskGeometry mask;
        if(filled){semantic::FilledPathMask path;path.path.commands={semantic::MoveTo{{80,48}},semantic::LineTo{{120,48}},semantic::LineTo{{120,80}},semantic::LineTo{{80,80}},semantic::ClosePath{}};mask=path;}
        else {semantic::SweptCircleMask sweep;sweep.segments.push_back({{{80,64},12},{{120,64},12},{93.333333,64},{106.666667,64}});mask=sweep;}
        erased.record.eraseMasks={{ObjectId::fromUint64(99003),mask}};
        std::vector entries{blue,erased};const auto frame=plan(99004,entries);
        TestSurface target;require(target.resize(256,128).code==BackendSubmissionCode::kAccepted,"mixed mask target");
        SkiaRenderer renderer;RenderCacheOptions options;options.navigationMaxSteps=256;options.navigationMicroseconds=1000000;renderer.setRenderCacheOptions(options);
        NavigationSourceQuery query=[&](const foundation::WorldRect&,std::vector<ReferenceTraversalEntry>& out){out=entries;return true;};
        for(const CanonicalViewportTransform camera: {CanonicalViewportTransform{1,0,0},{1,9,7},{1.25F,-18,-8}}){
            for(unsigned i=0;;++i){require(navigate(renderer,target,camera,frame.frame,query,false).code==BackendSubmissionCode::kAccepted,"mixed mask navigation");if(!renderer.navigationWork().pending)break;require(i<127,"mixed mask navigation convergence");}
            const auto actual=pixels(target);const auto x=unsigned(100*camera.scale+camera.translationX),y=unsigned(64*camera.scale+camera.translationY);const auto index=(y*256+x)*4;
            require(actual[index]<5&&actual[index+1]<5&&actual[index+2]>250,"navigation erase mask cleared the underlying blue object");
        }
        require(renderer.submissionCount()==0,"navigation issued a canonical receipt");
    }
}

namespace {
void checkLiveEraseSourceReuse() {
    TestSurface target,reference;
    require(target.resize(256,128).code==BackendSubmissionCode::kAccepted&&reference.resize(256,128).code==BackendSubmissionCode::kAccepted,"live erase targets");
    SkiaRenderer cached,oracle;RenderCacheOptions disabled;disabled.enabled=false;oracle.setRenderCacheOptions(disabled);
    std::vector<ReferenceTraversalEntry> sources;
    for(unsigned i=0;i<3;++i) {
        auto entry=stroke(90000+i);auto content=std::get<CompactStrokeReferenceCommand>(entry.command).content.get();
        content.stroke.config=*ink::BrushPackageCatalog{}.loadDefault("round-handdrawn",4).package.compactConfig;
        content.stroke.config.size=12;content.stroke.config.thinning=.4;content.stroke.config.opacity=.65;content.stroke.knots.clear();
        for(unsigned j=0;j<1001;++j)content.stroke.knots.push_back({12.+j*.21,55.+30*std::sin(j*.04),static_cast<std::uint16_t>(18000+j%300*100)});
        entry.command=CompactStrokeReferenceCommand{content};entry.record.content=content;
        entry.record.transform.tx=i*4;entry.record.transform.ty=i*5;entry.presentationEraseActive=true;sources.push_back(entry);
    }
    // Existing canonical erasure must not disable source reuse on the next drag.
    semantic::FilledPathMask old;old.path.commands={semantic::MoveTo{{38,0}},semantic::LineTo{{43,0}},semantic::LineTo{{43,128}},semantic::LineTo{{38,128}},semantic::ClosePath{}};
    sources[0].record.eraseMasks={{ObjectId::fromUint64(91000),old}};
    const auto initial=plan(900,sources);EditingOverlay overlay(initial.frame);
    require(cached.renderTransientOverlay(target,initial,&overlay).code==BackendSubmissionCode::kAccepted,"live erase warm source");
    const auto warmed=cached.renderCacheWork();require(warmed.entries==3,"translated/permanently masked sources not admitted");
    for(unsigned frame=0;frame<8;++frame) {
        for(auto& source:sources) {
            semantic::FilledPathMask mask;const double x=70+frame*7;
            mask.path.commands={semantic::MoveTo{{x,0}},semantic::LineTo{{x+25,0}},semantic::LineTo{{x+25,128}},semantic::LineTo{{x,128}},semantic::ClosePath{}};
            source.presentationEraseMasks={{ObjectId{},mask}};
        }
        auto direct=sources;
        for(auto& source:direct){source.record.eraseMasks.insert(source.record.eraseMasks.end(),source.presentationEraseMasks.begin(),source.presentationEraseMasks.end());source.presentationEraseMasks.clear();}
        const auto framePlan=plan(901+frame,sources),expected=plan(901+frame,direct);
        require(cached.renderTransientOverlay(target,framePlan,&overlay).code==BackendSubmissionCode::kAccepted&&oracle.renderTransientOverlay(reference,expected,&overlay).code==BackendSubmissionCode::kAccepted,"live erase frame");
        const auto actual=pixels(target),wanted=pixels(reference);
        for(std::size_t k=0;k<actual.size();++k)require(std::abs(int(actual[k])-int(wanted[k]))<=2,"live erase cached pixels differ from canonical oracle");
        require(cached.renderCacheWork().rasterizedGroups==warmed.rasterizedGroups,"growing erase trace rerasterized canonical stroke bodies");
        require(cached.renderCacheWork().coverageBuilds==warmed.coverageBuilds,"growing erase trace rebuilt canonical contour coverage");
    }
    for(auto& source:sources)source.presentationEraseMasks.clear();
    require(cached.renderTransientOverlay(target,plan(910,sources),&overlay).code==BackendSubmissionCode::kAccepted&&oracle.renderTransientOverlay(reference,plan(910,sources),&overlay).code==BackendSubmissionCode::kAccepted&&pixels(target)==pixels(reference),"cancel failed to restore canonical cached ink");
    // A canonical content edit still invalidates that source.
    auto changed=std::get<CompactStrokeReferenceCommand>(sources[1].command).content.get();changed.stroke.config.color={1,0,0,1};sources[1].command=CompactStrokeReferenceCommand{changed};sources[1].record.content=changed;
    require(cached.renderTransientOverlay(target,plan(911,sources),&overlay).code==BackendSubmissionCode::kAccepted&&cached.renderCacheWork().rasterizedGroups==warmed.rasterizedGroups+1,"canonical edit failed to invalidate erased source");
    semantic::SweptCircleMask bad;bad.segments.push_back({{{10,10},-1},{{20,20},-1},{13,13},{17,17}});sources[0].presentationEraseMasks={{ObjectId{},bad}};auto invalid=plan(912,sources);
    const auto presents=target.presentCount();require(cached.renderTransientOverlay(target,invalid,&overlay).code==BackendSubmissionCode::kRejected&&target.presentCount()==presents,"invalid transient mask presented");
}
}

int main(int argc,char** argv) {
    const bool gpuCandidate=argc>1&&std::string_view(argv[1])=="--gpu-candidate";
    try {
        if(argc==1||(argc>1&&std::string_view(argv[1])=="--live-erase-only")){checkLiveEraseSourceReuse();if(argc>1)return finishNative(0);}
        if(argc>1&&std::string_view(argv[1])=="--masked-mixed-only"){checkErasedInkOverShapeNavigation();return finishNative(0);}
        if(argc==1)checkErasedInkOverShapeNavigation();
#if defined(G5_TEST_WEBGL)
        if(argc>1&&std::string_view(argv[1])=="--stream-only"){testing::requireReferenceDrawStream();return finishNative(0);}
#endif
#if defined(G5_TEST_WEBGL)
        if(argc>1&&std::string_view(argv[1])=="--retirement-frame-only"){checkRetirementFrame();return finishNative(0);}
        if(argc>1&&std::string_view(argv[1])=="--retirement-cells-only"){checkRetirementCells();return finishNative(0);}
        if(argc>1&&std::string_view(argv[1])=="--retirement-only"){checkRetirementFrame();checkRetirementCells();return finishNative(0);}
#endif
        if(argc>1&&std::string_view(argv[1])=="--reentry-only"){checkRetainedZoomReentry();return finishNative(0);}
        if(argc==1)checkRetainedZoomReentry();
        TestSurface provider;
        require(provider.resize(256,128).code==BackendSubmissionCode::kAccepted,"surface");
        if(argc==1||(argc>1&&std::string_view(argv[1])=="--detail-only")){
            checkResidentNavigationDensity();checkObsoleteExactNavigation();checkSharedCanonicalNavigation(provider);checkSharedFrameAccounting(provider);checkCanonicalReplayHeadroom(provider);if(argc>1)return finishNative(0);
        }
        if(argc>1&&std::string_view(argv[1])=="--detail-obsolete-only"){checkObsoleteExactNavigation();return finishNative(0);}
        if(argc>1&&std::string_view(argv[1])=="--detail-residency-only"){checkResidentNavigationDensity();return finishNative(0);}
        if(argc>1&&std::string_view(argv[1])=="--detail-share-only"){checkSharedCanonicalNavigation(provider);checkSharedFrameAccounting(provider);return finishNative(0);}
        if(argc>1&&std::string_view(argv[1])=="--detail-replay-only"){checkCanonicalReplayHeadroom(provider);return finishNative(0);}
        if(argc==1||(argc>1&&std::string_view(argv[1])=="--oversized-only")){
            checkOversizedCanonical(provider);if(argc>1)return finishNative(0);
        }
        if(argc>1&&std::string_view(argv[1])=="--coverage-reuse-only"){checkCoverageReclaimFeasibility(provider);return finishNative(0);}
        if(argc>1&&std::string_view(argv[1])=="--dense-frontier-only"){checkDenseFrontier(provider);checkDenseFrontier(provider,true);checkFrontierRegionCompaction(provider);return finishNative(0);}
        if(argc==1||(argc>1&&std::string_view(argv[1])=="--frontier-only")){
            checkCoarseCameraStability();checkCoverageReclaimFeasibility(provider);checkDenseFrontier(provider);checkDenseFrontier(provider,true);checkFrontierRegionCompaction(provider);if(argc>1)return finishNative(0);
        }
#if defined(G5_TEST_WEBGL)
        if(argc>1&&std::string_view(argv[1])=="--exact-replay-only"){
            const auto acquired=provider.acquire();require(acquired.code==SkiaSurfaceAcquireCode::kAcquired,"exact replay acquire");
            testing::requireExactContourLocality(*acquired.frame.surface);testing::requireExactContourReplay(*acquired.frame.surface);provider.release();return finishNative(0);
        }
#endif
        if(argc==1||(argc>1&&std::string_view(argv[1])=="--replay-only")){
            const auto repaired=checkLargeReplayNavigation(provider);
            require(repaired==checkLargeReplayNavigation(provider,true),"local replay changed complete navigation feedback pixels");
            if(argc>1)return finishNative(0);
        }
        SkiaRenderer renderer;
        if(argc>1&&std::string_view(argv[1])=="--background-only"){
            TestSurface reference;require(reference.resize(256,128).code==BackendSubmissionCode::kAccepted,"background reference");
            checkRetainedBackground(provider,reference);return finishNative(0);
        }
        if(argc>1&&std::string_view(argv[1])=="--cold-quality-only"){
            TestSurface reference;require(reference.resize(256,128).code==BackendSubmissionCode::kAccepted,"cold quality reference");
            checkNavigationColdFrontier(provider,reference);return finishNative(0);
        }
        if(argc>1&&std::string_view(argv[1])=="--quality-only"){
            TestSurface reference;require(reference.resize(256,128).code==BackendSubmissionCode::kAccepted,"quality reference");
            checkNavigationSharpOverlap(provider,reference);checkNavigationColdFrontier(provider,reference);
            checkNavigationRefinementProgress(provider);checkExactCachePressure(provider,reference);return finishNative(0);
        }
        if(argc>1&&std::string_view(argv[1])=="--affine-only"){
            TestSurface reference;require(reference.resize(256,128).code==BackendSubmissionCode::kAccepted,"affine reference surface");
            checkRetainedBackground(provider,reference);
            checkExactCachePressure(provider,reference);
            checkAffineStrokes(provider,reference);return finishNative(0);
        }
#if defined(G5_TEST_WEBGL)
        if(argc>1&&std::string_view(argv[1])=="--contour-only"){
            const auto a=provider.acquire();require(a.code==SkiaSurfaceAcquireCode::kAcquired,"alpha oracle acquire");
            testing::requirePreparedAlphaMaskParity(*a.frame.surface);provider.release();checkGpuContours(provider);return finishNative(0);}
        {
            SkiaRenderer nav;RenderCacheOptions options;
            options.navigationMaxSteps=4096;options.navigationMicroseconds=1000000;
            options.navigationMaxGpuContourPrimitives=64;nav.setRenderCacheOptions(options);
            auto entry=stroke(884);auto c=std::get<CompactStrokeReferenceCommand>(entry.command).content.get();
            c.stroke.config=*ink::BrushPackageCatalog{}.loadDefault("round-handdrawn",4).package.compactConfig;
            c.stroke.config.smoothing=0;c.stroke.knots.clear();
            for(unsigned i=0;i<1024;++i)c.stroke.knots.push_back({30.+i*.17,64.+std::sin(i*.2)*20,32768});
            std::get<CompactStrokeReferenceCommand>(entry.command).content=c;entry.record.content=c;entry.record.worldBounds={0,0,256,128};const auto frame=plan(584,{entry});
            NavigationSourceQuery query=[&](const foundation::WorldRect&,std::vector<ReferenceTraversalEntry>& out){out={entry};return true;};
            for(unsigned i=0;;++i){
                const auto before=nav.navigationWork().gpuContourPrimitives;
                require(navigate(nav,provider,{1,0,0},frame.frame,query).code==BackendSubmissionCode::kAccepted,"GPU contour admission");
                require(nav.navigationWork().gpuContourPrimitives-before<=options.navigationMaxGpuContourPrimitives,
                        "CPU allowance admitted excessive GPU contour work");
                if(!nav.navigationWork().pending)break;require(i<127,"GPU contour admission stalled");
            }
            require(nav.navigationWork().gpuContourPrimitives>options.navigationMaxGpuContourPrimitives&&nav.navigationWork().coverageComplete,
                    "GPU admission oracle did not exercise multi-opportunity path work");
        }
        if(argc>1&&std::string_view(argv[1])=="--continuity-only"){
            testing::requireReferenceDrawStream();checkGpuContours(provider);return finishNative(0);
        }
#endif

        TestSurface reference;require(reference.resize(256,128).code==BackendSubmissionCode::kAccepted,"oracle surface");
        checkRetainedBackground(provider,reference);
        checkNavigationSharpOverlap(provider,reference);checkNavigationColdFrontier(provider,reference);
        checkNavigationRefinementProgress(provider);
        checkExactCachePressure(provider,reference);
        checkAffineStrokes(provider,reference);
        SkiaRenderer direct;RenderCacheOptions disabled;disabled.enabled=false;direct.setRenderCacheOptions(disabled);
        auto compare=[&](const FramePlan& frame){
            const auto rendered=renderer.renderFrame(provider,frame);
            if(rendered.code!=BackendSubmissionCode::kAccepted)std::cerr<<"rejected frame="<<frame.frame.frameId.value()<<" reason="<<rendered.message<<'\n';
            require(rendered.code==BackendSubmissionCode::kAccepted,"cached frame");
            require(direct.renderFrame(reference,frame).code==BackendSubmissionCode::kAccepted,"direct frame");
            const auto actual=pixels(provider),expected=pixels(reference);
            if(actual!=expected){unsigned differences=0,maxError=0;for(std::size_t i=0;i<actual.size();++i){differences+=actual[i]!=expected[i];maxError=std::max(maxError,unsigned(std::abs(int(actual[i])-int(expected[i]))));}
                std::cerr<<"frame="<<frame.frame.frameId.value()<<" different channels="<<differences<<" max="<<maxError<<'\n';}
            require(actual==expected,"cached/direct pixels differ");
        };
        {
            // Capacity, not object count, must not force unchanged coverage to
            // rerasterize on every transform. Exact preparation is the warmup.
            SkiaRenderer retained,oracle;oracle.setRenderCacheOptions(disabled);
            RenderCacheOptions bounded;bounded.maxEntries=2;retained.setRenderCacheOptions(bounded);
            std::vector<ReferenceTraversalEntry> sources;
            for(unsigned i=0;i<1000;++i)sources.push_back(stroke(20000+i,20+(i%6)*12));
            semantic::ObjectRecord object;object.id=ObjectId::fromUint64(30000);
            object.kind=semantic::ObjectKind::kShape;object.kind_version=1;
            object.content=semantic::ShapeContent{1,60,40};object.transform.tx=50;object.transform.ty=30;
            const auto projected=projectRuntimeScene({&object,1},semantic::SemanticGeneration{1});
            sources.push_back({projected.records.front(),true,ShapeReferenceCommand{std::get<semantic::ShapeContent>(object.content)}});
            RenderPreparationBudget slice;slice.maxGroups=64;slice.maxCoverageSteps=4096;slice.maxMicroseconds=1000000;
            const auto initial=plan(560,sources);BackendSubmissionResult result;
            unsigned attempts=0;
            do{result=retained.renderFrame(provider,initial,nullptr,&slice);require(++attempts<100,"1000 source warmup stalled");}
            while(result.code==BackendSubmissionCode::kRetry);
            require(result.code==BackendSubmissionCode::kAccepted,"1000 source warmup");
            const auto rasterized=retained.renderCacheWork().rasterizedGroups;

            EditingOverlay overlay(initial.frame);
            EditingOverlayInput selected;selected.selectedObject=object.id;selected.selected=true;selected.worldBounds=sources.back().record.worldBounds;
            require(overlay.update(selected),"1000 source shape selection");
            for(unsigned i=0;i<4;++i){
                sources.back().record.transform.tx+=7;
                const auto moved=plan(561+i,sources);
                require(retained.renderTransientOverlay(provider,moved,&overlay).code==BackendSubmissionCode::kAccepted,"1000 source live frame");
                require(oracle.renderTransientOverlay(reference,moved,&overlay).code==BackendSubmissionCode::kAccepted&&pixels(provider)==pixels(reference),"retained opaque prefix pixels differ");
                require(retained.renderCacheWork().rasterizedGroups==rasterized+i+1,"1000 source drag rerasterized unchanged background");
                require(retained.renderCacheWork().entries<=bounded.maxEntries,"interaction bypassed object capacity");
            }
        }
        {
            // The active stroke can be in the middle of accumulated ink. Its
            // transform must not invalidate both unchanged traversal segments.
            SkiaRenderer retained,oracle;oracle.setRenderCacheOptions(disabled);
            RenderCacheOptions bounded;bounded.maxEntries=2;retained.setRenderCacheOptions(bounded);
            std::vector<ReferenceTraversalEntry> sources;
            for(unsigned i=0;i<24;++i)sources.push_back(stroke(32000+i,20+(i%6)*12));
            const auto initial=plan(558,sources);EditingOverlay overlay(initial.frame);
            EditingOverlayInput selection;selection.selectedObject=sources[12].record.objectId;
            selection.selected=true;selection.worldBounds=sources[12].record.worldBounds;
            require(overlay.update(selection),"middle stroke selection");
            RenderPreparationBudget slice;slice.maxGroups=64;slice.maxCoverageSteps=4096;slice.maxMicroseconds=1000000;
            BackendSubmissionResult result;unsigned attempts=0;
            do{result=retained.renderFrame(provider,initial,&overlay,&slice);require(++attempts<64,"selected stroke preparation stalled");}
            while(result.code==BackendSubmissionCode::kRetry);
            require(result.code==BackendSubmissionCode::kAccepted,"selected stroke preparation");
            const auto rasterized=retained.renderCacheWork().rasterizedGroups;
            for(unsigned i=0;i<3;++i){
                sources[12].record.transform.tx+=5;
                const auto moved=plan(559+i,sources);
                require(retained.renderTransientOverlay(provider,moved,&overlay).code==BackendSubmissionCode::kAccepted&&oracle.renderTransientOverlay(reference,moved,&overlay).code==BackendSubmissionCode::kAccepted,"middle stroke drag");
                require(retained.renderCacheWork().rasterizedGroups<=rasterized+i+1,"active stroke invalidated unchanged front/back coverage");
                const auto actual=pixels(provider),expected=pixels(reference);unsigned maximum=0;
                for(std::size_t n=0;n<actual.size();++n)maximum=std::max(maximum,unsigned(std::abs(int(actual[n])-int(expected[n]))));
                require(maximum<=3,"middle stroke reordered retained content");
            }
        }
        {
            // Retaining all static content under the active object changes
            // occlusion. Runs on both sides must stay at their traversal slot.
            SkiaRenderer retained,oracle;oracle.setRenderCacheOptions(disabled);
            RenderCacheOptions bounded;bounded.maxEntries=2;retained.setRenderCacheOptions(bounded);
            semantic::ObjectRecord object;object.id=ObjectId::fromUint64(31000);
            object.kind=semantic::ObjectKind::kShape;object.kind_version=1;
            object.content=semantic::ShapeContent{1,80,70};object.transform.tx=60;object.transform.ty=25;
            const auto projected=projectRuntimeScene({&object,1},semantic::SemanticGeneration{1});
            ReferenceTraversalEntry shape{projected.records.front(),true,ShapeReferenceCommand{std::get<semantic::ShapeContent>(object.content)}};
            auto erased=stroke(31001);semantic::FilledPathMask mask;
            mask.path.commands={semantic::MoveTo{{80,0}},semantic::LineTo{{105,0}},semantic::LineTo{{105,128}},semantic::LineTo{{80,128}},semantic::ClosePath{}};
            std::vector sources{erased,stroke(31003,45),shape,stroke(31004,50),stroke(31005,55)};
            RenderPreparationBudget slice;slice.maxGroups=2;
            const auto warm=[&](const FramePlan& demand){
                BackendSubmissionResult r;unsigned attempts=0;
                do{r=retained.renderFrame(provider,demand,nullptr,&slice);require(++attempts<64,"ordered runs never warm");}
                while(r.code==BackendSubmissionCode::kRetry);
                require(r.code==BackendSubmissionCode::kAccepted&&oracle.renderFrame(reference,demand).code==BackendSubmissionCode::kAccepted&&pixels(provider)==pixels(reference),"canonical used approximate run pixels");
            };
            warm(plan(565,sources));const auto builds=retained.renderCacheWork().retainedRunBuilds;
            const auto overlayPlan=plan(565,sources);EditingOverlay overlay(overlayPlan.frame);
            for(unsigned i=0;i<3;++i){
                sources[2].record.transform.tx+=13;
                const auto moved=plan(566+i,sources);
                require(retained.renderTransientOverlay(provider,moved,&overlay).code==BackendSubmissionCode::kAccepted&&oracle.renderTransientOverlay(reference,moved,&overlay).code==BackendSubmissionCode::kAccepted,"ordered live frame");
                const auto actual=pixels(provider),expected=pixels(reference);unsigned maximum=0;
                for(std::size_t n=0;n<actual.size();++n)maximum=std::max(maximum,unsigned(std::abs(int(actual[n])-int(expected[n]))));
                require(maximum<=2,"ordered run changes occlusion/erase or excessive quantization");
            }
            require(retained.renderCacheWork().retainedRunBuilds==builds&&retained.renderCacheWork().retainedRunHits>=6,"transform rebuilt static front/back runs");
            const auto beforeFeedback=pixels(provider);
            sources[0].presentationOpacity=.2F;
            const auto dimmed=plan(569,sources);
            require(retained.renderTransientOverlay(provider,dimmed,&overlay).code==BackendSubmissionCode::kAccepted&&
                oracle.renderTransientOverlay(reference,dimmed,&overlay).code==BackendSubmissionCode::kAccepted,"feedback opacity presentation");
            const auto feedbackPixels=pixels(provider),directPixels=pixels(reference);
            require(feedbackPixels!=beforeFeedback,"cached run ignored candidate opacity");
            for(std::size_t n=0;n<feedbackPixels.size();++n)
                require(std::abs(int(feedbackPixels[n])-int(directPixels[n]))<=2,"cached feedback differs from Direct");
            sources[0].presentationOpacity=1;
            // A retained run must not conceal frame-only clipping metadata.
            sources[0].presentationEraseMasks={{ObjectId{},mask}};
            auto clippedOracle=sources;clippedOracle[0].record.eraseMasks.push_back({ObjectId::fromUint64(31003),mask});clippedOracle[0].presentationEraseMasks.clear();
            require(retained.renderTransientOverlay(provider,plan(569,sources),&overlay).code==BackendSubmissionCode::kAccepted&&oracle.renderTransientOverlay(reference,plan(569,clippedOracle),&overlay).code==BackendSubmissionCode::kAccepted,"retained run transient erase");
            const auto clippedPixels=pixels(provider),clippedExpected=pixels(reference);
            for(std::size_t n=0;n<clippedPixels.size();++n)require(std::abs(int(clippedPixels[n])-int(clippedExpected[n]))<=2,"retained run ignored transient erase");
            sources[0].presentationEraseMasks.clear();
            // Content, order, camera, target and resource changes must never
            // reuse old pixels, including remove/readd with a repeated ID.
            auto content=std::get<CompactStrokeReferenceCommand>(sources[0].command).content.get();
            sources[0].record.eraseMasks={{ObjectId::fromUint64(31002),mask}};
            content.stroke.knots[1].y=95;sources[0].command=CompactStrokeReferenceCommand{content};sources[0].record.content=content;
            warm(plan(570,sources));std::swap(sources[0],sources[4]);warm(plan(571,sources));
            warm(plan(572,sources,1,{1.25,0,0,1.25,-4,-8}));
            retained.setImageResolver(nullptr);warm(plan(573,sources));
            // Creation replays transparent ink runs, never the opaque prefix
            // snapshot (which would erase the newly authored object).
            overlay.setCreationPreview(object,{});const auto creation=plan(574,sources);
            require(retained.renderTransientOverlay(provider,creation,&overlay).code==BackendSubmissionCode::kAccepted&&oracle.renderTransientOverlay(reference,creation,&overlay).code==BackendSubmissionCode::kAccepted,"retained creation");
            const auto actual=pixels(provider),expected=pixels(reference);unsigned maximum=0;
            for(std::size_t n=0;n<actual.size();++n)maximum=std::max(maximum,unsigned(std::abs(int(actual[n])-int(expected[n]))));
            require(maximum<=3,"creation retained opaque backdrop or changed ink ordering");
            const auto presents=provider.presentCount();const auto generation=provider.generation();
            require(retained.renderTransientOverlay(provider,plan(575,sources,generation+1),&overlay).code==BackendSubmissionCode::kRejected&&provider.presentCount()==presents,"stale retained run presented");
            TestSurface other;require(other.resize(256,128).code==BackendSubmissionCode::kAccepted,"other target");
            require(retained.renderTransientOverlay(other,plan(576,sources),&overlay).code==BackendSubmissionCode::kAccepted&&oracle.renderTransientOverlay(reference,plan(576,sources),&overlay).code==BackendSubmissionCode::kAccepted&&pixels(other)==pixels(reference),"retained pixels crossed provider target");
        }
        {
            // One unsupported source must not force all background ink to
            // raster synchronously or publish the partially prepared target.
            semantic::ObjectRecord object;object.id=ObjectId::fromUint64(9701);
            object.kind=semantic::ObjectKind::kShape;object.kind_version=1;
            object.content=semantic::ShapeContent{1,70,45};object.transform.tx=70;object.transform.ty=50;
            const auto projected=projectRuntimeScene({&object,1},semantic::SemanticGeneration{1});
            ReferenceTraversalEntry shape{projected.records.front(),true,ShapeReferenceCommand{std::get<semantic::ShapeContent>(object.content)}};
            auto a=stroke(9702),b=stroke(9703,70),unsupported=stroke(9704,90);
            unsupported.record.transform.tx=3;
            for(bool chrome:{false,true}){
                SkiaRenderer mixed,oracle;oracle.setRenderCacheOptions(disabled);
                const auto demand=plan(570,{a,shape,b,unsupported});
                EditingOverlay overlay(demand.frame);RenderPreparationBudget slice;slice.maxGroups=1;slice.maxCoverageSteps=1;
                const auto before=pixels(provider);const auto presents=provider.presentCount();
                require(mixed.renderFrame(provider,demand,chrome?&overlay:nullptr,&slice).code==BackendSubmissionCode::kRetry,
                        "mixed scene bypassed cooperative ink preparation");
                require(pixels(provider)==before&&provider.presentCount()==presents,"mixed preparation published partial pixels");
                unsigned attempts=0;BackendSubmissionResult result;
                do{result=mixed.renderFrame(provider,demand,chrome?&overlay:nullptr,&slice);require(++attempts<32,"mixed preparation never converges");}
                while(result.code==BackendSubmissionCode::kRetry);
                require(result.code==BackendSubmissionCode::kAccepted&&mixed.renderCacheWork().preparationSlices>1,
                        "mixed preparation did not resume");
                require(oracle.renderFrame(reference,demand,chrome?&overlay:nullptr).code==BackendSubmissionCode::kAccepted&&pixels(provider)==pixels(reference),
                        "mixed prepared pixels differ from Direct");
                const auto misses=mixed.renderCacheWork().misses;
                require(mixed.renderFrame(provider,demand,chrome?&overlay:nullptr,&slice).code==BackendSubmissionCode::kAccepted&&
                        mixed.renderCacheWork().misses==misses,"mixed scene discarded reusable ink");
            }
            // Capacity denial is local too, including an otherwise valid
            // source larger than the retained budget. Keep order and masks.
            for(bool oversized:{false,true}){
                SkiaRenderer limited,oracle;oracle.setRenderCacheOptions(disabled);
                RenderCacheOptions options;options.maxEntries=1;
                if(oversized)options.maxEntryBytes=6000;
                limited.setRenderCacheOptions(options);
                auto large=stroke(9710);auto content=std::get<CompactStrokeReferenceCommand>(large.command).content.get();
                content.stroke.config.size=100;large.command=CompactStrokeReferenceCommand{content};large.record.content=content;
                auto small=stroke(9711);content=std::get<CompactStrokeReferenceCommand>(small.command).content;
                content.stroke.knots={{20,95,32768},{35,95,32768}};small.command=CompactStrokeReferenceCommand{content};small.record.content=content;
                const auto demand=plan(571,oversized?std::vector{large,shape,small}:std::vector{a,shape,b});
                RenderPreparationBudget slice;slice.maxCoverageSteps=1;
                BackendSubmissionResult result;unsigned attempts=0;
                const auto directBefore=limited.renderCacheWork().directGroups;
                do{result=limited.renderFrame(provider,demand,nullptr,&slice);require(++attempts<32,"capacity-limited prepare loops");}
                while(result.code==BackendSubmissionCode::kRetry);
                require(result.code==BackendSubmissionCode::kAccepted&&limited.renderCacheWork().preparationBudgetEntries>0&&
                        limited.renderCacheWork().entries==1,"capacity denial disabled all local preparation");
                require(oracle.renderFrame(reference,demand).code==BackendSubmissionCode::kAccepted&&pixels(provider)==pixels(reference),
                        "local capacity fallback changed pixels");
                require(limited.renderCacheWork().directGroups<=directBefore+1,
                        "budget fallback accumulated compact raster work in final submission");
                // Superseding a partial streamed frame must discard its prefix,
                // including order/background/resource decisions, before submit.
                auto pending=plan(572,{a,shape,b},1,{1.2,0,0,1.2,-5,-2});
                require(limited.renderFrame(provider,pending,nullptr,&slice).code==BackendSubmissionCode::kRetry,
                        "stream supersession setup");
                limited.setImageResolver(nullptr);
                auto erased=a;erased.record.objectId=ObjectId::fromUint64(9713);semantic::FilledPathMask erase;
                erase.path.commands={semantic::MoveTo{{40,0}},semantic::LineTo{{60,0}},semantic::LineTo{{60,128}},semantic::LineTo{{40,128}},semantic::ClosePath{}};
                erased.record.eraseMasks={{ObjectId::fromUint64(9712),erase}};erased.record.transform.tx=3;
                auto changed=b;auto material=std::get<CompactStrokeReferenceCommand>(changed.command).content.get();
                material.stroke.config.opacity=.3;changed.command=CompactStrokeReferenceCommand{material};changed.record.content=material;
                foundation::DocumentBackground backdrop;backdrop.color={.2F,.4F,.6F,1.F};
                auto replacement=plan(573,{changed,shape,erased,a},1,{1.1,0,0,1.1,-2,-1},{0,0,256,128},backdrop,true);
                attempts=0;
                do{result=limited.renderFrame(provider,replacement,nullptr,&slice);require(++attempts<32,"superseded stream loops");}
                while(result.code==BackendSubmissionCode::kRetry);
                require(result.code==BackendSubmissionCode::kAccepted&&oracle.renderFrame(reference,replacement).code==BackendSubmissionCode::kAccepted&&
                        pixels(provider)==pixels(reference),"superseded stream kept stale order/material/background");
                const auto retainedMisses=limited.renderCacheWork().misses;
                require(limited.renderFrame(provider,replacement,nullptr,&slice).code==BackendSubmissionCode::kAccepted&&
                        limited.renderCacheWork().misses==retainedMisses,
                        "identical streamed demand restarted after completion");
            }
        }
        {
            semantic::ObjectRecord object;object.id=ObjectId::fromUint64(880);
            object.kind=semantic::ObjectKind::kShape;object.kind_version=1;
            object.content=semantic::ShapeContent{1,80,50};object.transform.tx=80;object.transform.ty=65;
            const auto projection=projectRuntimeScene({&object,1},semantic::SemanticGeneration{1});
            ReferenceTraversalEntry shape{projection.records.front(),true,ShapeReferenceCommand{std::get<semantic::ShapeContent>(object.content)}};
            SkiaRenderer cached,oracle;oracle.setRenderCacheOptions(disabled);
            auto ink=stroke(879);auto initial=plan(580,{ink,shape});
            require(cached.renderFrame(provider,initial).code==BackendSubmissionCode::kAccepted,"warm canonical editing source");
            const auto canonicalMisses=cached.renderCacheWork().misses;
            EditingOverlay overlay(initial.frame);require(overlay.update({.selectedObject=object.id,.worldBounds=shape.record.worldBounds,.objectTransform=object.transform,.selected=true,.active=true}),"drag chrome");
            const auto compareOverlay=[&](const FramePlan& frame){
                require(cached.renderTransientOverlay(provider,frame,&overlay).code==BackendSubmissionCode::kAccepted&&
                        oracle.renderTransientOverlay(reference,frame,&overlay).code==BackendSubmissionCode::kAccepted&&
                        pixels(provider)==pixels(reference),"cached transient differs from Direct");
            };
            compareOverlay(initial);
            require(cached.renderCacheWork().misses==canonicalMisses&&cached.renderCacheWork().hits>0,
                    "first transient frame discarded canonical ink raster");
            require(cached.renderCacheWork().misses>0,"transient editing bypasses shared ink cache");
            const auto built=cached.renderCacheWork().misses;
            shape.record.transform.tx+=20;compareOverlay(plan(581,{ink,shape}));
            require(cached.renderCacheWork().misses==built&&cached.renderCacheWork().hits>0,"moving shape rerasterized unchanged ink");
            overlay.setCreationPreview(object,{});compareOverlay(plan(582,{ink}));
            const auto afterCreation=cached.renderCacheWork().misses;
            object.transform.tx+=10;overlay.setCreationPreview(object,{});compareOverlay(plan(583,{ink}));
            require(cached.renderCacheWork().misses==afterCreation,"creation replays unchanged ink Direct");
            compareOverlay(plan(584,{ink},1,{1.25,0,0,1.25,-5,-6}));
            auto masked=ink;semantic::FilledPathMask mask;
            mask.path.commands={semantic::MoveTo{{40,0}},semantic::LineTo{{60,0}},semantic::LineTo{{60,128}},semantic::LineTo{{40,128}},semantic::ClosePath{}};
            masked.record.eraseMasks={{ObjectId::fromUint64(881),mask}};
            compareOverlay(plan(585,{masked,shape}));
            cached.setImageResolver(nullptr);compareOverlay(plan(586,{masked,shape}));
        }
        {
            // Integration oracle for new brush evaluators through G5's
            // bounded canonical path, including material seed and AA contours.
            for(const auto profile:{"round-handdrawn","marker-compact-v1","crayon-wax-v1","chalk-compact-v1","watercolor-lite-v1"}){
                const auto revision=(std::string_view(profile)=="round-handdrawn"||std::string_view(profile)=="marker-compact-v1")?4:3;
                auto entry=stroke(885);
                auto content=std::get<CompactStrokeReferenceCommand>(entry.command).content.get();
                content.stroke.config=*ink::BrushPackageCatalog{}.loadDefault(profile,revision).package.compactConfig;
                content.stroke.config.size=18;content.stroke.config.opacity=.55;content.stroke.seed=19377;
                content.stroke.knots.clear();
                for(unsigned k=0;k<100;++k)content.stroke.knots.push_back({15.+k*2.1,64.+std::sin(k*.16)*23,static_cast<std::uint16_t>(18000+k*300)});
                entry.command=CompactStrokeReferenceCommand{content};std::get<CompactStrokeReferenceCommand>(entry.command).content=content;entry.record.content=content;
                SkiaRenderer cached,oracle;oracle.setRenderCacheOptions(disabled);
                auto frame=plan(590,{entry});RenderPreparationBudget budget;budget.maxCoverageSteps=1;
                BackendSubmissionResult result;unsigned slices=0;
                do{result=cached.renderFrame(provider,frame,nullptr,&budget);require(++slices<128,"new evaluator preparation does not converge");}
                while(result.code==BackendSubmissionCode::kRetry);
                require(result.code==BackendSubmissionCode::kAccepted&&slices>1,"new evaluator bypassed bounded coverage");
                require(oracle.renderFrame(reference,frame).code==BackendSubmissionCode::kAccepted&&pixels(provider)==pixels(reference),
                        "new evaluator resumed/cache pixels differ from Direct");
                const auto misses=cached.renderCacheWork().misses;
                const auto warm=plan(591,{entry});
                require(cached.renderFrame(provider,warm).code==BackendSubmissionCode::kAccepted&&
                        cached.renderCacheWork().misses==misses&&pixels(provider)==pixels(reference),"new evaluator warm cache invalid");
                SkiaRenderer navigation;RenderCacheOptions options;
                options.navigationMaxSteps=256;options.navigationMicroseconds=1000000;
                navigation.setRenderCacheOptions(options);
                NavigationSourceQuery query=[&](const foundation::WorldRect&,std::vector<ReferenceTraversalEntry>& entries){entries={entry};return true;};
                for(unsigned n=0;n<128;++n){
                    require(navigate(navigation,provider,{1,0,0},frame.frame,query).code==BackendSubmissionCode::kAccepted,
                            "new evaluator navigation rejected");
                    if(!navigation.navigationWork().pending)break;
                    require(n<127,"new evaluator navigation does not converge");
                }
                const auto feedback=pixels(provider);
                require(navigation.navigationWork().coverageComplete&&navigation.navigationWork().sourceSteps>0&&
                        navigation.submissionCount()==0&&std::any_of(feedback.begin(),feedback.end(),[](auto channel){return channel<240;}),
                        "new evaluator navigation missing coverage or manufactured canonical work");
                if(content.stroke.config.algorithmVersion==4){
                    const auto work=navigation.navigationWork();
                    require(work.preparedGeometryBuilds==1&&work.preparedGeometryHits>0&&
                            work.preparedGeometryBytes>0&&work.contourPrimitivesDrawn>0,
                            "v4 navigation did not use shared budgeted contour geometry");
                }
            }
        }
        {
            // A partial erase changes only the masked stroke payload. The
            // navigation feedback lane must still be able to retain and
            // reproject that stroke instead of rejecting the whole source
            // set and falling back to an exact full-scene render.
            auto masked = stroke(886);
            semantic::SweptCircleMask erase;
            erase.segments.push_back({{{80.0, 64.0}, 12.0},
                                      {{120.0, 64.0}, 12.0},
                                      {93.333333, 64.0}, {106.666667, 64.0}});
            masked.record.eraseMasks = {{ObjectId::fromUint64(887), erase}};
            SkiaRenderer navigation;
            RenderCacheOptions options;
            options.navigationMaxSteps = 256;
            options.navigationMicroseconds = 1000000;
            navigation.setRenderCacheOptions(options);
            const auto frame = plan(592, {masked});
            NavigationSourceQuery query = [&](const foundation::WorldRect&,
                                               std::vector<ReferenceTraversalEntry>& entries) {
                entries = {masked};
                return true;
            };
            for (unsigned n = 0; n < 128; ++n) {
                require(navigate(navigation, provider, {1, 0, 0}, frame.frame, query).code ==
                            BackendSubmissionCode::kAccepted,
                        "masked stroke navigation rejected");
                if (!navigation.navigationWork().pending) break;
                require(n < 127, "masked stroke navigation does not converge");
            }
            require(navigation.navigationWork().coverageComplete &&
                        navigation.navigationWork().sourceSteps > 0 &&
                        navigation.submissionCount() == 0,
                    "masked stroke fell back to canonical rendering during navigation");
        }
#if defined(G5_TEST_WEBGL)
        // Multiple device-grid boundaries, partial edge extents, transparent
        // material and resource denial are exercised on the actual GL context.
        {
            const auto acquired=provider.acquire();auto* canvas=acquired.frame.surface->getCanvas();
            const auto info=SkImageInfo::MakeN32Premul(600,300);
            for(const auto family:{foundation::InkFamily::kRound,foundation::InkFamily::kMarker})
            for(unsigned transform=0;transform<3;++transform){
                auto gpu=canvas->makeSurface(info),oracle=canvas->makeSurface(info);
                foundation::InkBrushConfig config;config.version=2;config.algorithmVersion=3;config.signalVersion=2;
                config.family=family;config.size=80;config.opacity=.37;config.startTaper=35;config.endTaper=60;
                std::vector<foundation::InkKnot> knots;
                for(unsigned k=0;k<70;++k)knots.push_back({-40.+k*10,145.+std::sin(k*.27)*65,static_cast<std::uint16_t>(10000+k*700)});
                for(auto* surface:{gpu.get(),oracle.get()}){
                    auto* target=surface->getCanvas();target->clear(SK_ColorTRANSPARENT);
                    target->translate(transform==0?0.F:17.3F,-5.2F);target->scale(transform==1?.8F:1.2F,transform==2?.9F:1.2F);
                    target->clipRect(SkRect::MakeLTRB(4,3,550,270));
                }
                internal::CompactCoverageTargets targets;internal::CompactRasterWork work;
                auto denied=std::make_shared<resources::ResourceBudgetManager>(resources::BudgetLimits{1,1});
                internal::CompactStrokeRasterJob job(*gpu->getCanvas(),config,knots,nullptr,true,transform==2?denied:nullptr);
                while(!job.step(*gpu->getCanvas(),config,knots,77,targets,true,&work)){}
                internal::drawCompactStrokeToSkCanvas(*oracle->getCanvas(),config,knots,77);
                std::vector<std::uint8_t> a(600*300*4),b(a.size());
                require(gpu->readPixels(info,a.data(),2400,0,0)&&oracle->readPixels(info,b.data(),2400,0,0),"seam readback");
                require(a==b,"hybrid device tile seam differs from Direct");
                if(transform==2)require(work.gpuTiles==0,"denied region budget still admitted hybrid work");
            }
            provider.release();
            checkGpuContours(provider);
        }
#endif
        RenderCacheOptions tiled;tiled.tileCandidate=true;tiled.tileWorldSize=64;
        renderer.setRenderCacheOptions(tiled);
        const auto tileStroke=stroke(8);
        compare(plan(500,{tileStroke}));
        const auto tileRasters=renderer.renderCacheWork().tileRasters;
        compare(plan(501,{tileStroke}));
        require(tileRasters>1&&renderer.renderCacheWork().tileRasters==tileRasters,"clean tiles rerasterized");
        require(renderer.renderCacheWork().tileHits>0,"tile source not used");
        compare(plan(502,{tileStroke},1,{1.2,0,0,1.2,-6.3,-2.8}));
        for(const unsigned material:{1U,2U})for(const auto family:{foundation::InkFamily::kRound,foundation::InkFamily::kMarker,foundation::InkFamily::kChalk,foundation::InkFamily::kCrayon,foundation::InkFamily::kWatercolor}){
            auto entry=tileStroke;auto content=std::get<CompactStrokeReferenceCommand>(entry.command).content.get();
            content.stroke.config.family=family;content.stroke.config.materialVersion=material;
            content.stroke.config.materialResourceId=material==1?foundation::kInkMaterialResourceId:foundation::kInkMaterialV2ResourceId;
            content.stroke.config.materialResourceSha256=material==1?foundation::kInkMaterialResourceSha256:foundation::kInkMaterialV2ResourceSha256;
            entry.command=CompactStrokeReferenceCommand{content};std::get<CompactStrokeReferenceCommand>(entry.command).content=content;entry.record.content=content;
            compare(plan(510,{entry}));compare(plan(511,{entry}));
        }
        compare(plan(512,{tileStroke,stroke(9,55)}));compare(plan(513,{stroke(9,55)}));
        renderer.clearRenderCache();compare(plan(514,{tileStroke}));
        foundation::DocumentBackground tileBackground;tileBackground.color={.2F,.4F,.7F,1.F};
        compare(plan(515,{tileStroke},1,{}, {0,0,256,128},tileBackground));
        compare(plan(516,{tileStroke},1,{}, {24,8,232,112},tileBackground));
        compare(plan(517,{tileStroke},1,{}, {0,0,256,128},tileBackground));
        compare(plan(518,{tileStroke},1,{}, {0,0,256,128},{},true));
        {RenderCacheOptions options;options.gpuCoverageCandidate=gpuCandidate;renderer.setRenderCacheOptions(options);}
        const auto a=stroke(1);
#if defined(G5_TEST_WEBGL)
        if(gpuCandidate){
            auto tuned=a;auto c=std::get<CompactStrokeReferenceCommand>(tuned.command).content.get();
            c.stroke.config.version=2;c.stroke.config.algorithmVersion=3;c.stroke.config.signalVersion=2;
            std::get<CompactStrokeReferenceCommand>(tuned.command).content=c;tuned.record.content=c;
            compare(plan(590,{tuned}));
            require(renderer.renderCacheWork().gpuCoverageTiles>0,"candidate did not execute GPU coverage");
            std::cout<<"candidate GPU coverage tiles="<<renderer.renderCacheWork().gpuCoverageTiles<<'\n';
        }
#endif
        // Cheap checkpoints should consume the available owner time instead
        // of imposing an extra display frame after only 2048 input knots.
        // An explicit single-step budget below still protects cancellation.
        {
            auto longContour=stroke(887);
            auto content=std::get<CompactStrokeReferenceCommand>(longContour.command).content.get();
            content.stroke.config.version=2;content.stroke.config.algorithmVersion=4;content.stroke.config.signalVersion=3;
            content.stroke.knots.clear();
            for(unsigned i=0;i<4096;++i)
                content.stroke.knots.push_back({20.+i*.05,45.+std::sin(i*.02)*12,32768});
            std::get<CompactStrokeReferenceCommand>(longContour.command).content=content;longContour.record.content=content;
            renderer.clearRenderCache();
            RenderPreparationBudget available;available.maxMicroseconds=1000000;
            const auto demand=plan(588,{longContour});
            const auto steps=renderer.renderCacheWork().preparationCoverageSteps;
            const auto progress=renderer.renderFrame(provider,demand,nullptr,&available);
            require(progress.code!=BackendSubmissionCode::kRejected,"progress fixture is invalid");
            require(progress.code==BackendSubmissionCode::kAccepted,
                    "cheap exact checkpoints yielded with owner time still available");
            require(renderer.renderCacheWork().preparationCoverageSteps>steps+64,
                    "progress guard did not exercise the former 64-step ceiling");
            require(direct.renderFrame(reference,demand).code==BackendSubmissionCode::kAccepted&&
                    pixels(provider)==pixels(reference),"larger checkpoint allowance changed exact contour pixels");
        }
        {
            // A long smoothed contour fits the unchanged per-entry budget;
            // knot-count-only worst-case path admission used to deny it and
            // replay all knots for every world tile instead of sharing paths.
            auto longEntry=stroke(889);auto content=std::get<CompactStrokeReferenceCommand>(longEntry.command).content.get();
            content.stroke.config=*ink::BrushPackageCatalog{}.loadDefault("round-handdrawn",4).package.compactConfig;
            content.stroke.knots.clear();
            for(unsigned i=0;i<16384;++i)content.stroke.knots.push_back({40.+i*.035,64.+std::sin(i*.035)*42,32768});
            std::get<CompactStrokeReferenceCommand>(longEntry.command).content=content;longEntry.record.content=content;longEntry.record.worldBounds={0,0,660,128};
            SkiaRenderer nav;RenderCacheOptions options;options.navigationMaxSteps=8;options.navigationMicroseconds=1000000;
            nav.setRenderCacheOptions(options);const auto frame=plan(589,{longEntry});
            NavigationSourceQuery query=[&](const foundation::WorldRect&,std::vector<ReferenceTraversalEntry>& out){out={longEntry};return true;};
            require(navigate(nav,provider,{1,-16,0},frame.frame,query).code==BackendSubmissionCode::kAccepted,"long contour acquisition");
            require(nav.navigationWork().preparedGeometryBuilds==1&&nav.navigationWork().preparedGeometryBytes>0&&
                    nav.navigationWork().preparedGeometryBytes<=options.maxEntryBytes,
                    "long contour denied despite bounded materialized path storage");
        }
        // Denied prepared geometry must resume the pinned scalar evaluator,
        // including the full-stroke distance/taper phase after each checkpoint.
        {
            auto entry=stroke(886);auto content=std::get<CompactStrokeReferenceCommand>(entry.command).content.get();
            content.stroke.config=*ink::BrushPackageCatalog{}.loadDefault("round-handdrawn",4).package.compactConfig;
            content.stroke.config.smoothing=0;content.stroke.knots.clear();
            for(unsigned i=0;i<512;++i)content.stroke.knots.push_back({20.+i*.4,64.+std::sin(i*.15)*20,32768});
            std::get<CompactStrokeReferenceCommand>(entry.command).content=content;entry.record.content=content;entry.record.worldBounds={0,0,256,128};
            SkiaRenderer nav;RenderCacheOptions options;options.navigationMaxSteps=1;options.navigationMicroseconds=1000000;
            nav.setRenderCacheOptions(options);const auto frame=plan(586,{entry});
            NavigationSourceQuery query=[&](const foundation::WorldRect&,std::vector<ReferenceTraversalEntry>& out){out={entry};return true;};
            require(navigate(nav,provider,{1,0,0},frame.frame,query).code==BackendSubmissionCode::kAccepted,"contour checkpoint setup");
            const auto before=nav.navigationWork();
            require(navigate(nav,provider,{1,0,0},frame.frame,query).code==BackendSubmissionCode::kAccepted,"contour checkpoint progress");
            const auto after=nav.navigationWork();
            require(after.contourPrimitivesTested>before.contourPrimitivesTested&&
                    after.contourPrimitivesTested-before.contourPrimitivesTested<=32&&after.knotSteps==before.knotSteps,
                    "prepared contour bypassed bounded primitive checkpoints");
        }
        {
            auto denied=stroke(888);auto sBuilder=std::get<CompactStrokeReferenceCommand>(denied.command).content.get();auto& s=sBuilder.stroke;
            s.config.version=2;s.config.algorithmVersion=3;s.config.signalVersion=2;
            s.config.startTaper=80;s.config.endTaper=90;s.knots.clear();
            for(unsigned i=0;i<20000;++i)s.knots.push_back({-20.+i*.05,64.+std::sin(i*.001)*12,32768});
            std::get<CompactStrokeReferenceCommand>(denied.command).content=sBuilder;denied.record.content=std::get<CompactStrokeReferenceCommand>(denied.command).content;
            SkiaRenderer navigation;RenderCacheOptions options;options.maxEntryBytes=260*260*4*3;
            options.navigationMaxSteps=4096;options.navigationMicroseconds=1000000;
            navigation.setRenderCacheOptions(options);const auto frame=plan(587,{denied});
            NavigationSourceQuery query=[&](const foundation::WorldRect&,std::vector<ReferenceTraversalEntry>& out){out={denied};return true;};
            for(unsigned i=0;;++i){
                require(navigate(navigation,provider,{1,0,0},frame.frame,query).code==BackendSubmissionCode::kAccepted,"denied geometry feedback");
                if(!navigation.navigationWork().pending)break;
                require(i<128,"denied geometry fallback stalled");
            }
            require(navigation.navigationWork().preparedGeometryDenied>0,"denial fixture did not deny geometry");
            require(direct.renderFrame(reference,frame).code==BackendSubmissionCode::kAccepted&&
                    pixels(provider)==pixels(reference),"denied geometry lost full-stroke distance phase");
        }
        // One long source must yield within the stroke; no intermediate target
        // pixels or receipt may escape, including on camera supersession.
        {
            // Conservative Scene visibility can include geometry wholly outside
            // the acquired clip. It must not abandon cooperative preparation
            // and synchronously draw every other source.
            auto invisible=stroke(890,-200);invisible.record.worldBounds={0,-250,256,128};
            auto visible=stroke(891),other=stroke(892,70);
            renderer.clearRenderCache();RenderPreparationBudget one;one.maxGroups=1;
            one.maxMicroseconds=1000000;one.maxCoverageSteps=1;
            const auto demand=plan(589,{invisible,visible,other});
            const auto presents=provider.presentCount();
            require(renderer.renderFrame(provider,demand,nullptr,&one).code==BackendSubmissionCode::kRetry&&
                    provider.presentCount()==presents,"offscreen candidate forced synchronous exact fallback");
            unsigned tries=0;BackendSubmissionResult result;
            do{result=renderer.renderFrame(provider,demand,nullptr,&one);require(++tries<100,"offscreen preparation did not converge");}
            while(result.code==BackendSubmissionCode::kRetry);
            require(result.code==BackendSubmissionCode::kAccepted&&direct.renderFrame(reference,demand).code==BackendSubmissionCode::kAccepted&&
                    pixels(provider)==pixels(reference),"offscreen preparation changed exact pixels");
            const auto moved=plan(590,{invisible,visible,other},1,{1,0,0,1,0,240});
            do{result=renderer.renderFrame(provider,moved,nullptr,&one);require(++tries<200,"offscreen admission camera update");}
            while(result.code==BackendSubmissionCode::kRetry);
            require(result.code==BackendSubmissionCode::kAccepted&&direct.renderFrame(reference,moved).code==BackendSubmissionCode::kAccepted&&
                    pixels(provider)==pixels(reference),"offscreen admission survived camera change");
        }
        {
            auto longStroke=stroke(91);
            auto content=std::get<CompactStrokeReferenceCommand>(longStroke.command).content.get();
            content.stroke.config.version=2;content.stroke.config.algorithmVersion=3;content.stroke.config.signalVersion=2;
            content.stroke.config.startTaper=25;content.stroke.config.endTaper=35;
            content.stroke.knots.clear();
            for(unsigned i=0;i<240;++i)content.stroke.knots.push_back({20.+i*.85,40.+std::sin(i*.06)*15,static_cast<std::uint16_t>(12000+i*170)});
            std::get<CompactStrokeReferenceCommand>(longStroke.command).content=content;longStroke.record.content=content;
            renderer.clearRenderCache();
            const auto previous=pixels(provider);const auto presents=provider.presentCount();
            const auto stepsBefore=renderer.renderCacheWork().preparationCoverageSteps;
            const auto allocationsBefore=renderer.renderCacheWork().coverageTargetAllocations;
            RenderPreparationBudget bounded;bounded.maxGroups=1;bounded.maxMicroseconds=1000000;bounded.maxCoverageSteps=1;
            const auto demand=plan(595,{longStroke},1,{1.2,0,0,1.2,-6.3,-2.8});
            require(renderer.renderFrame(provider,demand,nullptr,&bounded).code==BackendSubmissionCode::kRetry,
                    "single long stroke monopolizes an exact preparation opportunity");
            require(provider.presentCount()==presents&&pixels(provider)==previous,"partial stroke escaped preparation");
            require(renderer.renderCacheWork().preparationCoverageSteps==stepsBefore+1,"single-step admission exceeded");
            require(renderer.renderCacheWork().coverageTargetAllocations<=allocationsBefore+4,"temporary coverage residency unbounded");
            auto replacement=plan(596,{longStroke},1,{.9,0,0,.9,8.7,4.3});
            unsigned retries=0;BackendSubmissionResult result;
            do{result=renderer.renderFrame(provider,replacement,nullptr,&bounded);++retries;require(retries<200,"resumed stroke never converges");}
            while(result.code==BackendSubmissionCode::kRetry);
            require(result.code==BackendSubmissionCode::kAccepted&&retries>1,"replacement stroke was not cooperative");
            require(direct.renderFrame(reference,replacement).code==BackendSubmissionCode::kAccepted&&pixels(provider)==pixels(reference),
                    "resumed tapered coverage differs from full-history Direct");
            // Same source identity, changed canonical payload: discard partial
            // coverage even when the camera and target generation did not move.
            renderer.clearRenderCache();
            require(renderer.renderFrame(provider,demand,nullptr,&bounded).code==BackendSubmissionCode::kRetry,"content replacement setup");
            auto edited=longStroke;
            auto editedContent=std::get<CompactStrokeReferenceCommand>(edited.command).content.get();
            editedContent.stroke.knots[100].y+=35;std::get<CompactStrokeReferenceCommand>(edited.command).content=editedContent;edited.record.content=editedContent;
            const auto editedDemand=plan(596,{edited},1,{1.2,0,0,1.2,-6.3,-2.8});
            retries=0;
            do{result=renderer.renderFrame(provider,editedDemand,nullptr,&bounded);require(++retries<200,"changed source never converges");}
            while(result.code==BackendSubmissionCode::kRetry);
            require(result.code==BackendSubmissionCode::kAccepted&&direct.renderFrame(reference,editedDemand).code==BackendSubmissionCode::kAccepted&&
                    pixels(provider)==pixels(reference),"partial coverage survived content replacement");
            const auto allocations=renderer.renderCacheWork().coverageTargetAllocations;
            compare(plan(597,{longStroke},1,{1.1,0,0,1.1,-3.1,2.7}));
            require(renderer.renderCacheWork().coverageTargetAllocations==allocations,
                    "camera rebuild allocates another compatible coverage target");
            require(renderer.renderCacheWork().coverageTargetReuses>0,"coverage target never reused");
#if defined(G5_TEST_WEBGL)
            if(gpuCandidate){
                const auto work=renderer.renderCacheWork();
                require(work.gpuInteriorPixels>0&&work.cpuEdgePixels>0,"hybrid did not separate exact edges from saturated interior");
                std::cout<<"hybrid interior="<<work.gpuInteriorPixels<<" exact-edge="<<work.cpuEdgePixels<<'\n';
            }
#endif
        }
        // Exact preparation is cooperative and offscreen: no partial pixels,
        // present or canonical submission is published between bounded slices.
        compare(plan(600,{a}));
        const auto beforePrepare=pixels(provider);
        const auto preparePresents=provider.presentCount();
        const auto prepareSubmissions=renderer.submissionCount();
        renderer.clearRenderCache();
        RenderPreparationBudget slice; slice.maxGroups=1; slice.maxMicroseconds=1000000;
        const std::vector sources{a,stroke(11,45),stroke(12,60),stroke(13,75)};
        const auto preparePlan=plan(601,sources,1,{1.2,0,0,1.2,-6.3,-2.8});
        for(unsigned i=0;i<3;++i){
            const auto builds=renderer.renderCacheWork().rasterizedGroups;
            require(renderer.renderFrame(provider,preparePlan,nullptr,&slice).code==BackendSubmissionCode::kRetry,
                    "exact rebuild was not split into bounded opportunities");
            require(renderer.renderCacheWork().rasterizedGroups==builds+1,"preparation exceeded group budget");
            require(provider.presentCount()==preparePresents&&renderer.submissionCount()==prepareSubmissions,
                    "partial preparation published canonical work");
            require(pixels(provider)==beforePrepare,"preparation changed presented target");
        }
        require(renderer.renderFrame(provider,preparePlan,nullptr,&slice).code==BackendSubmissionCode::kAccepted,
                "exact preparation did not converge");
        require(direct.renderFrame(reference,preparePlan).code==BackendSubmissionCode::kAccepted,
                "prepared oracle frame");
        require(pixels(provider)==pixels(reference),"prepared frame differs from Direct");
        require(renderer.renderCacheWork().preparationSlices>=4,"preparation work not observed");
        // A newer camera supersedes pending exact pixels. Prepared rasters at
        // the previous transform must not qualify the replacement frame.
        renderer.clearRenderCache();
        require(renderer.renderFrame(provider,preparePlan,nullptr,&slice).code==BackendSubmissionCode::kRetry,
                "supersession setup did not yield");
        const auto replacement=plan(602,sources,1,{.9,0,0,.9,8.7,4.3});
        for(unsigned i=0;i<4;++i){
            const auto result=renderer.renderFrame(provider,replacement,nullptr,&slice);
            require(result.code==(i==3?BackendSubmissionCode::kAccepted:BackendSubmissionCode::kRetry),
                    "camera change reused obsolete preparation");
        }
        require(direct.renderFrame(reference,replacement).code==BackendSubmissionCode::kAccepted&&
                pixels(provider)==pixels(reference),"replacement camera differs from Direct");
        // Capacity-denied sources stream in order without requiring residency for all objects.
        RenderCacheOptions tooSmall;tooSmall.maxEntries=1;renderer.setRenderCacheOptions(tooSmall);
        {
            BackendSubmissionResult result;unsigned attempts=0;
            do{result=renderer.renderFrame(provider,preparePlan,nullptr,&slice);
                require(++attempts<32,"insufficient cache capacity did not converge");
            }while(result.code==BackendSubmissionCode::kRetry);
            require(result.code==BackendSubmissionCode::kAccepted,"capacity stream rejected");
        }
        require(direct.renderFrame(reference,preparePlan).code==BackendSubmissionCode::kAccepted&&
                pixels(provider)==pixels(reference),"capacity fallback differs from Direct");
        {RenderCacheOptions options;options.gpuCoverageCandidate=gpuCandidate;renderer.setRenderCacheOptions(options);}
        auto malformed=sources;std::get<CompactStrokeReferenceCommand>(malformed[1].command).content.update([](auto& c){c.stroke.config.size=-1;});
        const auto invalidBefore=pixels(provider);const auto invalidPresents=provider.presentCount();
        require(renderer.renderFrame(provider,plan(603,malformed),nullptr,&slice).code==BackendSubmissionCode::kRejected,
                "invalid geometry admitted by preparation");
        require(pixels(provider)==invalidBefore&&provider.presentCount()==invalidPresents,
                "invalid preparation changed presented target");
        for(unsigned material:{1U,2U})for(auto family:{foundation::InkFamily::kRound,foundation::InkFamily::kMarker,foundation::InkFamily::kChalk,foundation::InkFamily::kCrayon,foundation::InkFamily::kWatercolor}){
            renderer.clearRenderCache();auto varied=sources;
            for(auto& entry:varied){auto content=std::get<CompactStrokeReferenceCommand>(entry.command).content.get();
                content.stroke.config.family=family;content.stroke.config.materialVersion=material;
                content.stroke.config.materialResourceId=material==1?foundation::kInkMaterialResourceId:foundation::kInkMaterialV2ResourceId;
                content.stroke.config.materialResourceSha256=material==1?foundation::kInkMaterialResourceSha256:foundation::kInkMaterialV2ResourceSha256;
                entry.command=CompactStrokeReferenceCommand{content};std::get<CompactStrokeReferenceCommand>(entry.command).content=content;entry.record.content=content;
            }
            const auto frame=plan(604,varied,1,{1.2,0,0,1.2,-6.3,-2.8});
            for(unsigned i=0;i<4;++i)
                require(renderer.renderFrame(provider,frame,nullptr,&slice).code==
                    (i==3?BackendSubmissionCode::kAccepted:BackendSubmissionCode::kRetry),"material preparation did not converge");
            require(direct.renderFrame(reference,frame).code==BackendSubmissionCode::kAccepted&&
                    pixels(provider)==pixels(reference),"prepared material differs from Direct");
        }
        renderer.clearRenderCache();
        TestSurface lifecycleProvider;
        require(lifecycleProvider.resize(256,128).code==BackendSubmissionCode::kAccepted,"lifecycle target");
        require(renderer.renderFrame(lifecycleProvider,preparePlan,nullptr,&slice).code==BackendSubmissionCode::kRetry,
                "loss preparation setup");
        require(lifecycleProvider.lose().code==BackendSubmissionCode::kAccepted,"pending preparation loss");
        const auto lostPresents=lifecycleProvider.presentCount();
        require(renderer.renderFrame(lifecycleProvider,preparePlan,nullptr,&slice).code!=BackendSubmissionCode::kAccepted&&
                lifecycleProvider.presentCount()==lostPresents,"lost preparation presented");
        require(lifecycleProvider.resize(256,128).code==BackendSubmissionCode::kAccepted,"pending preparation restore");
        require(renderer.renderFrame(lifecycleProvider,preparePlan,nullptr,&slice).code==BackendSubmissionCode::kRejected,
                "old preparation generation accepted");
        renderer.clearRenderCache();
        // World sources cover newly exposed content after zooming out. The
        // old view deliberately does not contain this stroke's full extent.
        {
            SkiaRenderer worldRenderer;
            auto wide=stroke(901);auto wideStrokeBuilder=std::get<CompactStrokeReferenceCommand>(wide.command).content.get();auto& wideStroke=wideStrokeBuilder.stroke;
            wideStroke.knots={{-240,50,32768},{500,50,32768}};std::get<CompactStrokeReferenceCommand>(wide.command).content=wideStrokeBuilder;wide.record.content=std::get<CompactStrokeReferenceCommand>(wide.command).content;
            wide.record.worldBounds={-260,30,520,70};
            auto exactFrame=plan(700,{wide});
            require(worldRenderer.renderFrame(provider,exactFrame).code==BackendSubmissionCode::kAccepted,"world exact setup");
            const auto presents=provider.presentCount(),submissions=worldRenderer.submissionCount();
            NavigationSourceQuery query=[&](const foundation::WorldRect&,std::vector<ReferenceTraversalEntry>& entries){entries={wide};return true;};
            for(unsigned n=0;n<128;++n){
                require(navigate(worldRenderer,provider,{.5F,64,32},exactFrame.frame,query).code==BackendSubmissionCode::kAccepted,
                        "world navigation rejected");
                if(!worldRenderer.navigationWork().pending)break;
                require(n<127,"world navigation never completes");
            }
            const auto image=pixels(provider);
            // world x=-80 -> view x=24; outside the old [0,256] screenshot.
            require(image[(57*256+24)*4]<180,"zoom out lost content outside old viewport");
            require(worldRenderer.submissionCount()==submissions&&provider.presentCount()>presents,
                    "world navigation manufactured canonical submission");
            require(worldRenderer.navigationWork().tilesBuilt>0,"world tiles not built");
            const auto cleanPresents=provider.presentCount();
            require(navigate(worldRenderer,provider,{.5F,64,32},exactFrame.frame,query).code==BackendSubmissionCode::kAccepted&&
                    provider.presentCount()==cleanPresents,"clean navigation presented twice");
        }
        {
            // A supported long stroke must keep sharing the navigation tile
            // lane when a normal shape is present in the same viewport. The
            // shape is authored in local coordinates and carries the same
            // placement transform that canonical rendering applies.
            SkiaRenderer mixedRenderer;
            auto longEntry = stroke(902);
            auto longStrokeBuilder=std::get<CompactStrokeReferenceCommand>(longEntry.command).content.get();auto& longStroke=longStrokeBuilder.stroke;
            longStroke.knots = {{-120, 58, 32768}, {420, 58, 32768}};
            std::get<CompactStrokeReferenceCommand>(longEntry.command).content=longStrokeBuilder;longEntry.record.content = std::get<CompactStrokeReferenceCommand>(longEntry.command).content;
            longEntry.record.worldBounds = {-140, 38, 440, 78};
            semantic::ObjectRecord shapeObject;
            shapeObject.id = ObjectId::fromUint64(903);
            shapeObject.kind = semantic::ObjectKind::kShape;
            shapeObject.kind_version = 1;
            shapeObject.content = semantic::ShapeContent{1, 90, 54};
            shapeObject.transform.tx = 64;
            shapeObject.transform.ty = 32;
            const auto shapeProjection = projectRuntimeScene({&shapeObject, 1}, semantic::SemanticGeneration{1});
            ReferenceTraversalEntry shapeEntry{
                shapeProjection.records.front(), true,
                ShapeReferenceCommand{std::get<semantic::ShapeContent>(shapeObject.content)}};
            const auto mixedFrame = plan(701, {longEntry, shapeEntry});
            NavigationSourceQuery mixedQuery = [&](const foundation::WorldRect&,
                                                   std::vector<ReferenceTraversalEntry>& entries) {
                entries = {longEntry, shapeEntry};
                return true;
            };
            for (unsigned n = 0; n < 128; ++n) {
                require(navigate(mixedRenderer, provider, {1.2F, -12.0F, -8.0F},
                                 mixedFrame.frame, mixedQuery).code == BackendSubmissionCode::kAccepted,
                        "mixed stroke and shape navigation rejected");
                if (!mixedRenderer.navigationWork().pending) break;
                require(n < 127, "mixed stroke and shape navigation never converged");
            }
            require(mixedRenderer.navigationWork().coverageComplete &&
                        mixedRenderer.navigationWork().tilesBuilt > 0 &&
                        mixedRenderer.navigationWork().sourceSteps > 0,
                    "mixed stroke and shape bypassed navigation tiles");
        }
        {
            // Retina pinch crosses a scale bucket immediately. The feedback
            // plane must reduce resolution to fit its budget, not abandon
            // navigation for an unbounded synchronous exact render.
            RasterSkiaSurfaceProvider retina;
            require(retina.resize(1600,1000).code==BackendSubmissionCode::kAccepted,"retina target");
            SkiaRenderer nav;
            const FrameState current{ViewId{1},{},{0,0,800,500},{800,500,1600,1000,2,1},
                semantic::SemanticGeneration{705},SceneRevision{705},SurfaceGeneration{1},MetricsGeneration{1},FrameId{705}};
            NavigationSourceQuery empty=[](const foundation::WorldRect&,std::vector<ReferenceTraversalEntry>& e){e.clear();return true;};
            const auto submissions=nav.submissionCount();
            for(unsigned n=0;n<128;++n){
                require(navigate(nav,retina,{1.02F,-8,-5},current,empty).code==BackendSubmissionCode::kAccepted,
                        "retina pinch fell back to synchronous exact drawing");
                if(!nav.navigationWork().pending)break;
                require(n<127,"budget-adapted navigation never converged");
            }
            require(nav.navigationWork().coverageComplete&&retina.presentCount()>0&&nav.submissionCount()==submissions,
                    "budget-adapted feedback missing or qualified canonical");
            require(nav.navigationWork().retainedBytes<=RenderCacheOptions{}.maxBytes,"retina navigation exceeded budget");
        }
        {
            // A cold viewport needs a complete coarse cover before fine
            // refinement, even when an individual owner quantum is small.
            // This is a work-count oracle, independent of machine timing.
            RasterSkiaSurfaceProvider target;
            require(target.resize(1600,1000).code==BackendSubmissionCode::kAccepted,"coarse target");
            SkiaRenderer nav;RenderCacheOptions options;
            options.navigationMaxSteps=8;options.navigationMaxQueries=4;options.navigationMicroseconds=1000000;
            nav.setRenderCacheOptions(options);
            FrameState frame{ViewId{1},{},{0,0,800,500},{800,500,1600,1000,2,1},
                semantic::SemanticGeneration{706},SceneRevision{706},SurfaceGeneration{1},MetricsGeneration{1},FrameId{706}};
            NavigationSourceQuery empty=[](const foundation::WorldRect&,std::vector<ReferenceTraversalEntry>& entries){entries.clear();return true;};
            require(navigate(nav,target,{1,-16,-8},frame,empty).code==BackendSubmissionCode::kAccepted,"coarse opportunity");
            require(nav.navigationWork().coverageComplete&&nav.navigationWork().presented&&target.presentCount()==1,
                    "cold viewport waited for fine cells before coarse feedback");
            require(nav.navigationWork().pending&&nav.submissionCount()==0,
                    "coarse feedback skipped refinement or manufactured canonical work");
            require(navigate(nav,target,{1,-160,-80},frame,empty).code==BackendSubmissionCode::kAccepted&&
                    nav.navigationWork().presented&&target.presentCount()==2,
                    "nearby pan lost the complete coarse cover");
        }
        {
            FailingPresent failed;require(failed.resize(256,128).code==BackendSubmissionCode::kAccepted,"feedback failure target");
            failed.fail=false;SkiaRenderer nav;RenderCacheOptions options;options.navigationMaxSteps=1;
            nav.setRenderCacheOptions(options);const auto exact=plan(708,{stroke(907)});
            require(nav.renderFrame(failed,exact).code==BackendSubmissionCode::kAccepted,"feedback failure exact");
            failed.fail=true;
            NavigationSourceQuery query=[](const foundation::WorldRect&,std::vector<ReferenceTraversalEntry>& e){e={stroke(907)};return true;};
            require(navigate(nav,failed,{1.2F,-25.6F,-12.8F},exact.frame,query).code!=BackendSubmissionCode::kAccepted,
                    "failed retained feedback present reported accepted");
        }
        {
            TestSurface feedback;require(feedback.resize(256,128).code==BackendSubmissionCode::kAccepted,"cold feedback target");
            SkiaRenderer nav;RenderCacheOptions options;options.navigationMaxSteps=1;options.navigationMicroseconds=1000000;nav.setRenderCacheOptions(options);
            auto entry=stroke(908);auto exact=plan(709,{entry});
            require(nav.renderFrame(feedback,exact).code==BackendSubmissionCode::kAccepted,"cold feedback exact");
            const auto before=pixels(feedback);
            const auto presents=feedback.presentCount(),submissions=nav.submissionCount();
            bool sawQuery=false;
            NavigationSourceQuery query=[&](const foundation::WorldRect&,std::vector<ReferenceTraversalEntry>& e){
                require(feedback.presentCount()>presents,"cold source query ran before latest-camera feedback");
                sawQuery=true;e={entry};return true;};
            require(navigate(nav,feedback,{1.2F,-25.6F,-12.8F},exact.frame,query).code==BackendSubmissionCode::kAccepted,
                    "cold pinch feedback rejected");
            require(sawQuery&&nav.navigationWork().pending&&nav.navigationWork().presented&&feedback.presentCount()==presents+1,
                    "cold pinch froze while world tiles were preparing");
            require(pixels(feedback)!=before&&nav.submissionCount()==submissions,"cold feedback did not move pixels or qualified canonical");
            // A viewport screenshot may supply feedback only inside its known
            // coverage. Zoom-out must keep the complete display until world
            // coverage exists, never bring back the rectangular crop.
            const auto prior=feedback.presentCount();
            require(navigate(nav,feedback,{.5F,64,32},exact.frame,query).code==BackendSubmissionCode::kAccepted,
                    "cold zoom-out rejected");
            require(!nav.navigationWork().coverageComplete&&feedback.presentCount()==prior,
                    "partial screenshot was stretched over newly exposed content");
        }
        {
            TestSurface longSurface;require(longSurface.resize(256,128).code==BackendSubmissionCode::kAccepted,"long target");
            SkiaRenderer nav;RenderCacheOptions bounded;bounded.navigationMaxSteps=8;nav.setRenderCacheOptions(bounded);
            auto longEntry=stroke(910);auto content=std::get<CompactStrokeReferenceCommand>(longEntry.command).content.get();
            content.stroke.knots.clear();
            for(unsigned k=0;k<4096;++k)content.stroke.knots.push_back({-100.+k*.1,64,32768});
            std::get<CompactStrokeReferenceCommand>(longEntry.command).content=content;longEntry.record.content=content;longEntry.record.worldBounds={-130,35,340,93};
            auto current=plan(710,{longEntry});
            NavigationSourceQuery query=[&](const foundation::WorldRect&,std::vector<ReferenceTraversalEntry>& e){e={longEntry};return true;};
            require(navigate(nav,longSurface,{1,0,0},current.frame,query).code==BackendSubmissionCode::kAccepted,"long navigation");
            require(nav.navigationWork().pending&&nav.navigationWork().knotSteps<=256,
                    "long stroke did not yield inside coverage");
            require(nav.navigationWork().prefixScanKnots <= 4096,
                    "long stroke prefix work was not bounded");
            const auto beforeKnot=nav.navigationWork().knotSteps;
            // Slight camera motion does not obsolete a world-addressed job.
            require(navigate(nav,longSurface,{1,-.1F,0},current.frame,query).code==BackendSubmissionCode::kAccepted,
                    "world job camera update");
            require(nav.navigationWork().knotSteps>beforeKnot,"camera update starved world job");
            const auto cancelled=nav.navigationWork().cancelledJobs;
            const auto discarded=nav.navigationWork().discardedKnotSteps;
            require(navigate(nav,longSurface,{1,-10000,0},current.frame,query).code==BackendSubmissionCode::kAccepted,"obsolete world demand");
            require(nav.navigationWork().cancelledJobs>cancelled&&nav.navigationWork().discardedKnotSteps>discarded,
                    "obsolete world job was retained or discarded work unaccounted");
            // Old work must not publish after target loss or resource change.
            require(longSurface.lose().code==BackendSubmissionCode::kAccepted,"nav lost");
            require(navigate(nav,longSurface,{.5F,0,0},current.frame,query).code!=BackendSubmissionCode::kAccepted,
                    "lost navigation accepted");
            require(longSurface.resize(256,128).code==BackendSubmissionCode::kAccepted,"nav restore");
            require(navigate(nav,longSurface,{1,0,0},current.frame,query).code==BackendSubmissionCode::kRejected,
                    "old nav generation accepted");
        }
        {
            SkiaRenderer nav;auto solid=stroke(930);semantic::BrushStrokeContent vector;
            vector.stroke.snapshot.paint={0,0,0,1,.5,1,1};
            vector.stroke.vector_output.outline={{-200,30},{600,30},{600,90},{-200,90}};
            solid.command=BrushStrokeReferenceCommand{vector};solid.record.content=vector;
            solid.record.kindVersion=2;solid.record.worldBounds={-200,30,600,90};
            auto current=plan(720,{solid});
            NavigationSourceQuery query=[&](const foundation::WorldRect&,std::vector<ReferenceTraversalEntry>& e){e={solid};return true;};
            for(unsigned n=0;n<128;++n){
                require(navigate(nav,provider,{1,0,0},current.frame,query).code==BackendSubmissionCode::kAccepted,"vector navigation");
                if(!nav.navigationWork().pending)break;require(n<127,"vector nav loop");
            }
            const auto image=pixels(provider);
            for(unsigned x:{1U,63U,127U,128U,129U,254U})
                require(image[(60*256+x)*4]>=126&&image[(60*256+x)*4]<=129,"gutter or fallback double-blended vector ink");
            const auto built=nav.navigationWork().tilesBuilt;
            // Pan exposes at most one new column; interior world rasters stay reusable.
            require(navigate(nav,provider,{1,-1,0},current.frame,query).code==BackendSubmissionCode::kAccepted,"world reuse pan");
            require(nav.navigationWork().tilesBuilt<=built+2,"camera pan rebuilt all world rasters");
            nav.setImageResolver(nullptr);
            require(navigate(nav,provider,{1,-1,0},current.frame,query).code==BackendSubmissionCode::kAccepted&&
                    nav.navigationWork().tilesBuilt<=(built+8),"resource reset unusable");
        }
        {
            SkiaRenderer nav;auto entry=stroke(950);auto frame=plan(730,{entry});
            NavigationSourceQuery query=[&](const foundation::WorldRect&,std::vector<ReferenceTraversalEntry>& e){e={entry};return true;};
            for(unsigned n=0;n<128;++n){
                require(navigate(nav,provider,{1,-10,0},frame.frame,query).code==BackendSubmissionCode::kAccepted,"parent setup");
                if(!nav.navigationWork().pending)break;require(n<127,"parent setup loop");
            }
            RenderCacheOptions singleStep;singleStep.navigationMaxSteps=1;nav.setRenderCacheOptions(singleStep);
            // Rebuild the base plane with deterministic one-quantum admission.
            for(unsigned n=0;n<256;++n){
                require(navigate(nav,provider,{1,-10,0},frame.frame,query).code==BackendSubmissionCode::kAccepted,"fallback base");
                if(!nav.navigationWork().pending)break;require(n<255,"fallback base loop");
            }
            const auto fallbackBefore=nav.navigationWork().fallbackUses;
            require(navigate(nav,provider,{1.5F,-12,0},frame.frame,query).code==BackendSubmissionCode::kAccepted,
                    "parent scale switch");
            require(nav.navigationWork().fallbackUses>fallbackBefore,"ready parent coverage not selected");
            // Changed sources in the same world cells invalidate retained pixels.
            auto configBuilder=std::get<CompactStrokeReferenceCommand>(entry.command).content.get();auto& config=configBuilder.stroke.config;
            config.color={1,0,0,1};std::get<CompactStrokeReferenceCommand>(entry.command).content=configBuilder;entry.record.content=std::get<CompactStrokeReferenceCommand>(entry.command).content;
            auto next=plan(731,{entry});
            for(unsigned n=0;n<128;++n){
                require(navigate(nav,provider,{1,0,0},next.frame,query).code==BackendSubmissionCode::kAccepted,"new content nav");
                if(!nav.navigationWork().pending)break;require(n<127,"new content loop");
            }
            const auto red=pixels(provider);require(red[(50*256+80)*4]>red[(50*256+80)*4+2],"stale world content survived scene change");
            RenderCacheOptions tiny;tiny.maxEntries=1;nav.setRenderCacheOptions(tiny);
            const auto presents=provider.presentCount();
            require(navigate(nav,provider,{1,0,0},next.frame,query).code!=BackendSubmissionCode::kAccepted&&
                    provider.presentCount()==presents,"nonconvergent residency published navigation");
        }
        {
            SkiaRenderer nav;
            auto entry=stroke(960);auto invalid=plan(740,{entry});
            NavigationSourceQuery malformed=[&](const foundation::WorldRect&,std::vector<ReferenceTraversalEntry>& e){
                e={entry,entry};return true;
            };
            const auto presents=provider.presentCount();
            require(navigate(nav,provider,{1,0,0},invalid.frame,malformed).code!=BackendSubmissionCode::kAccepted&&
                    provider.presentCount()==presents,"duplicate world source double-painted");
        }
        {
            // With no complete feedback, a generous time/query grant can
            // prepare multiple cheap cells; one-cell admission freezes cold views.
            SkiaRenderer nav;RenderCacheOptions options;options.navigationMicroseconds=1000000;
            options.navigationMaxQueries=8;nav.setRenderCacheOptions(options);
            const auto frame=plan(749,{stroke(969)});
            NavigationSourceQuery empty=[](const foundation::WorldRect&,std::vector<ReferenceTraversalEntry>& e){e.clear();return true;};
            require(navigate(nav,provider,{1,0,0},frame.frame,empty).code==BackendSubmissionCode::kAccepted,"cold grant");
            require(nav.navigationWork().coverageComplete&&nav.navigationWork().initializations>1,"cold setup ignored available time/query grant");
        }
        {
            // Work policy includes new Scene queries and cold target setup.
            SkiaRenderer nav;RenderCacheOptions options;options.navigationMicroseconds=1000000;
            options.navigationMaxQueries=1;options.navigationPrewarmTiles=2;nav.setRenderCacheOptions(options);
            auto frame=plan(750,{stroke(970)});unsigned calls=0;
            NavigationSourceQuery empty=[&](const foundation::WorldRect&,std::vector<ReferenceTraversalEntry>& e){++calls;e.clear();return true;};
            for(unsigned i=0;i<128;++i){
                const auto old=calls;
                const auto initializations=nav.navigationWork().initializations;
                require(navigate(nav,provider,{1,0,0},frame.frame,empty).code==BackendSubmissionCode::kAccepted,"query-bound navigation");
                require(calls<=old+1&&nav.navigationWork().initializations<=initializations+1,"one opportunity exceeded query/initialization admission");
                if(!nav.navigationWork().pending)break;require(i<127,"query-bounded navigation never converges");
            }
            const auto levelChanges=nav.navigationWork().levelChanges;
            for(float scale:{1.01F,.99F,1.02F,.98F})
                require(navigate(nav,provider,{scale,0,0},frame.frame,empty).code==BackendSubmissionCode::kAccepted,"scale boundary navigation");
            require(nav.navigationWork().levelChanges==levelChanges,"scale boundary jitter rebuilds adjacent levels");
            // Finish current visible work, then explicitly grant idle work.
            for(unsigned i=0;i<128;++i){
                require(navigate(nav,provider,{1,0,0},frame.frame,empty).code==BackendSubmissionCode::kAccepted,"settled navigation");
                if(!nav.navigationWork().pending)break;require(i<127,"visible work never completes");
            }
            const auto presents=provider.presentCount(),submissions=nav.submissionCount();
            for(unsigned i=0;i<16;++i)require(navigate(nav,provider,{1,0,0},frame.frame,empty,true).code==BackendSubmissionCode::kAccepted,"idle prewarm");
            require(nav.navigationWork().prewarmTiles>0&&nav.navigationWork().prewarmTiles<=2,"idle prewarm is absent or unbounded");
            require(nav.navigationWork().retainedBytes<=options.maxBytes&&nav.submissionCount()==submissions,"prewarm exceeded residency or created canonical work");
            require(provider.presentCount()<=presents+2,"idle prewarm repeatedly presented unchanged pixels");
            const auto oldCalls=calls;
            for(unsigned i=0;i<4;++i)require(navigate(nav,provider,{1,0,0},frame.frame,empty,true).code==BackendSubmissionCode::kAccepted,"clean idle");
            require(calls==oldCalls,"bounded prewarm continued admitting cold work");
        }
#if defined(G5_TEST_WEBGL)
        {
            // GPU is allowed to lag: zero-timeout completion polls may refuse
            // further refinement; latest-camera composition must still present.
            TestSurface surface;require(surface.resize(256,128).code==BackendSubmissionCode::kAccepted,"capacity target");
            SkiaRenderer nav;RenderCacheOptions options;options.navigationMaxSteps=1;options.navigationMicroseconds=1000000;
            nav.setRenderCacheOptions(options);const auto frame=plan(760,{stroke(980)});
            require(nav.renderFrame(surface,frame).code==BackendSubmissionCode::kAccepted,"capacity exact");
            NavigationSourceQuery empty=[](const foundation::WorldRect&,std::vector<ReferenceTraversalEntry>& e){e.clear();return true;};
            EM_ASM({const p=WebGL2RenderingContext.prototype;window.g5OriginalPoll=p.clientWaitSync;
                p.clientWaitSync=function(...args){window.g5MaximumWait=Math.max(window.g5MaximumWait||0,args[2]);const result=window.g5OriginalPoll.apply(this,args);return result===this.WAIT_FAILED?result:this.TIMEOUT_EXPIRED;};});
            const auto presents=surface.presentCount();
            for(unsigned i=0;i<12;++i)require(navigate(nav,surface,{1.1F+float(i)*.002F,-14,-7},frame.frame,empty).code==BackendSubmissionCode::kAccepted,"capacity feedback");
            EM_ASM({WebGL2RenderingContext.prototype.clientWaitSync=window.g5OriginalPoll;});
            require(nav.navigationWork().gpuThrottles>0&&surface.presentCount()>presents+5,"GPU backlog blocks camera feedback or refinement is unbounded");
            require(EM_ASM_INT({return window.g5MaximumWait||0;})==0,"navigation waits on GPU completion");
            require(nav.submissionCount()==1,"capacity feedback created canonical submission");
        }
#endif
        // Navigation feedback is a separate submit and cannot manufacture a
        // canonical frame identity. Exact rendering after it must match Direct.
        compare(plan(90,{a}));
        const auto exactRaster=renderer.rasterizationCount();
        const auto oldPresent=provider.presentCount();
        const auto beforeNavigation=pixels(provider);
        require(navigate(renderer,provider,{1.1F,4.F,-3.F},
            plan(90,{a}).frame).code==BackendSubmissionCode::kAccepted,
            "navigation reprojection unavailable");
        require(provider.presentCount()==oldPresent+1 && renderer.rasterizationCount()==exactRaster,
            "navigation rebuilt canonical stroke geometry");
        require(pixels(provider)!=beforeNavigation,"navigation present did not move visible pixels");
        auto stale=plan(91,{a});
        require(navigate(renderer,provider,{1.1F,4.F,-3.F},stale.frame).code!=BackendSubmissionCode::kAccepted,
            "navigation reused another scene revision");
        compare(plan(92,{a},1,{1.1,0,0,1.1,4,-3}));
        renderer.clearRenderCache();

        {
            // Retaining a selected frame must cache content before chrome.
            // Reprojection then draws current-camera handles once, at their
            // logical size, rather than scaling stale handles from the image.
            const auto base=plan(93,{});
            EditingOverlay oldChrome(base.frame);
            EditingOverlayInput selected;
            selected.selectedObject=ObjectId::fromUint64(7601);
            selected.selected=true;selected.worldBounds={80,45,140,85};
            require(oldChrome.update(selected),"selected feedback chrome setup");
            SkiaRenderer selectedRenderer;
            require(selectedRenderer.renderFrame(provider,base,&oldChrome).code==BackendSubmissionCode::kAccepted,"selected feedback exact");
            auto zoomed=plan(93,{},1,{1.5,0,0,1.5,0,0});
            const FrameState overlayFrame{ViewId{1},CameraState{{256.F/3,128.F/3},1.5F,0,CameraGeneration{2}},
                {0,0,256,128},{256,128,256,128,1,1},semantic::SemanticGeneration{93},
                SceneRevision{93},SurfaceGeneration{1},MetricsGeneration{1},FrameId{93}};
            EditingOverlay currentChrome(overlayFrame);
            require(currentChrome.update(selected),"selected feedback zoomed chrome setup");
            require(navigate(selectedRenderer,provider,{1.5F,0,0},base.frame,&currentChrome).code==BackendSubmissionCode::kAccepted,
                    "selected feedback discarded content snapshot");
            require(direct.renderFrame(reference,zoomed,&currentChrome).code==BackendSubmissionCode::kAccepted&&pixels(provider)==pixels(reference),
                    "selected feedback scales stale chrome or draws handles twice");
            NavigationSourceQuery empty=[](const foundation::WorldRect&,std::vector<ReferenceTraversalEntry>& out){out.clear();return true;};
            unsigned steps=0;
            do{
                require(navigate(selectedRenderer,provider,{1.5F,0,0},base.frame,empty,false,&currentChrome).code==BackendSubmissionCode::kAccepted,"selected tile feedback");
                require(++steps<128,"selected tile feedback stalled");
            }while(selectedRenderer.navigationWork().pending);
            require(pixels(provider)==pixels(reference),"selected tile feedback omitted current selection chrome");
            require(selectedRenderer.submissionCount()==1,"selected navigation created canonical submission");
        }

        compare(plan(1,{a}));
        const auto first=renderer.rasterizationCount();
        const auto original=pixels(provider);
        compare(plan(2,{a}));
        require(renderer.rasterizationCount()==first,"unchanged canonical stroke rasterized again");
        require(pixels(provider)==original,"cache changed pixels");
        // Camera changes must reuse local coverage commands, while the
        // final device-space raster remains exact to the permanent Direct path.
        const auto recorded=renderer.renderCacheWork().coverageBuilds;
        for(unsigned i=0;i<8;++i)
            compare(plan(100+i,{a},1,{1.+(i+1)*.025,0,0,1.+(i+1)*.025,3.2*(i+1),-1.7*(i+1)}));
        require(recorded>0 && renderer.renderCacheWork().coverageBuilds==recorded,
                "camera change rebuilt compact coverage commands");
        require(renderer.renderCacheWork().coverageHits>=8,
                "camera-independent source was not reused");
        compare(plan(2,{a}));
        // Device raster work above is expected; append reuses this exact frame.
        const auto afterCamera=renderer.rasterizationCount();
        const auto scratchCount=renderer.renderCacheWork().scratchAllocations;
        const auto b=stroke(2,50);
        compare(plan(3,{a,b}));
        require(renderer.rasterizationCount()==afterCamera+1,"append rerasterized unchanged content");
        require(renderer.renderCacheWork().scratchAllocations==scratchCount,
                "each cache miss allocated another viewport scratch");
        compare(plan(4,{b,a}));
        require(renderer.rasterizationCount()==afterCamera+1,"external order invalidates raster unnecessarily");
        compare(plan(5,{b}));
        auto moved=b;moved.record.transform.tx=5;compare(plan(6,{moved}));
        auto masked=moved;semantic::FilledPathMask mask;
        mask.path.commands={semantic::MoveTo{{100,0}},semantic::LineTo{{140,0}},semantic::LineTo{{140,128}},semantic::LineTo{{100,128}},semantic::ClosePath{}};
        masked.record.eraseMasks={{ObjectId::fromUint64(99),mask}};compare(plan(7,{masked}));
        compare(plan(8,{masked},1,{}, {50,10,200,100}));
        compare(plan(81,{a,b},1,{}, {0,0,256,128}));
        compare(plan(9,{a,b},1,{1.25,0,0,1.25,-12.3,-6.7}));
        foundation::DocumentBackground background;background.color={.3F,.2F,.8F,1};
        compare(plan(10,{a,b},1,{}, {0,0,256,128},background));
        compare(plan(11,{a,b},1,{}, {0,0,256,128},{},true));
        renderer.clearRenderCache();compare(plan(12,{a,b}));
        RenderCacheOptions tiny;tiny.maxBytes=1;tiny.maxEntryBytes=1;renderer.setRenderCacheOptions(tiny);
        compare(plan(13,{a,b}));require(renderer.renderCacheWork().retainedBytes==0,"budget exceeded");
        {RenderCacheOptions options;options.gpuCoverageCandidate=gpuCandidate;renderer.setRenderCacheOptions(options);}
        for(unsigned material:{1U,2U})for(auto family:{foundation::InkFamily::kRound,foundation::InkFamily::kMarker,foundation::InkFamily::kChalk,foundation::InkFamily::kCrayon,foundation::InkFamily::kWatercolor}){
            auto entry=a;auto content=std::get<CompactStrokeReferenceCommand>(entry.command).content.get();
            content.stroke.config.family=family;content.stroke.config.materialVersion=material;
            content.stroke.config.materialResourceId=material==1?foundation::kInkMaterialResourceId:foundation::kInkMaterialV2ResourceId;
            content.stroke.config.materialResourceSha256=material==1?foundation::kInkMaterialResourceSha256:foundation::kInkMaterialV2ResourceSha256;
            entry.command=CompactStrokeReferenceCommand{content};std::get<CompactStrokeReferenceCommand>(entry.command).content=content;entry.record.content=content;
            for(const auto& transform:std::vector<WorldToViewAffine>{{},{.8,.3,-.2,.9,40,10},{-1,0,0,1,256,0},{2,0,0,2,-100,-40}}){
                compare(plan(20,{entry},1,transform));compare(plan(21,{entry},1,transform));
            }
        }
        // Tuned algorithms, caps/tapers and DPR must use the same exact
        // coverage in synchronous and resumable paths. In particular V3 AA
        // depends on the acquired matrix, not a picture recorded at identity.
        for(const auto family:{foundation::InkFamily::kRound,foundation::InkFamily::kMarker})
        for(const bool caps:{false,true}){
            auto entry=stroke(101);
            auto c=std::get<CompactStrokeReferenceCommand>(entry.command).content.get();
            c.stroke.config.version=2;c.stroke.config.algorithmVersion=3;c.stroke.config.signalVersion=2;
            c.stroke.config.family=family;c.stroke.config.startCap=caps;c.stroke.config.endCap=caps;
            c.stroke.config.startTaper=caps?40:0;c.stroke.config.endTaper=caps?70:0;
            c.stroke.knots.clear();
            for(unsigned i=0;i<40;++i)c.stroke.knots.push_back({18.+i*5,55.+std::sin(i*.7)*14,static_cast<std::uint16_t>(7000+i*1300)});
            std::get<CompactStrokeReferenceCommand>(entry.command).content=c;entry.record.content=c;
            for(const auto transform:std::vector<WorldToViewAffine>{{1.2,0,0,1.2,-6.3,-2.8},{.8,.3,-.2,.9,40,10}}){
                compare(plan(211,{entry},1,transform));
                compare(plan(212,{entry},1,transform,{0,0,128,64},{},false,2));
            }
            renderer.clearRenderCache();RenderPreparationBudget bounded;bounded.maxCoverageSteps=1;bounded.maxMicroseconds=1000000;
            const auto demand=plan(213,{entry},1,{1.17,0,0,1.17,-20.3,-4.8});
            unsigned tries=0;BackendSubmissionResult result;
            do{result=renderer.renderFrame(provider,demand,nullptr,&bounded);require(++tries<300,"tuned preparation did not converge");}
            while(result.code==BackendSubmissionCode::kRetry);
            require(result.code==BackendSubmissionCode::kAccepted,"tuned preparation rejected");
            require(direct.renderFrame(reference,demand).code==BackendSubmissionCode::kAccepted&&pixels(provider)==pixels(reference),
                    "resumed cap/taper differs from Direct");
        }
        // Deterministic adversarial tuned corpus: crossing segments, changing
        // radius, fractional device origins, nib rotation and alpha. Candidate
        // must stay exactly equal; legacy and tiny/thin paths may fall back.
        for(unsigned sample=0;sample<48;++sample){
            auto entry=stroke(121);
            auto c=std::get<CompactStrokeReferenceCommand>(entry.command).content.get();
            c.stroke.config.version=2;c.stroke.config.algorithmVersion=3;c.stroke.config.signalVersion=2;
            c.stroke.config.family=sample%2?foundation::InkFamily::kMarker:foundation::InkFamily::kRound;
            c.stroke.config.materialVersion=2;c.stroke.config.materialResourceId=foundation::kInkMaterialV2ResourceId;
            c.stroke.config.materialResourceSha256=foundation::kInkMaterialV2ResourceSha256;
            c.stroke.config.size=sample%4==0?1:sample%4==1?16:sample%4==2?64:4096;
            c.stroke.config.startCap=sample%3!=0;c.stroke.config.endCap=sample%3!=1;
            c.stroke.config.startTaper=sample%3==0?0:20;c.stroke.config.endTaper=sample%3==1?0:55;
            c.stroke.config.thinning=sample%3==0?1:sample%3==1?-.7:0;
            c.stroke.config.markerAngle=std::fmod(sample*.23,6.283185307179586);c.stroke.config.markerFlatness=sample%3==0?.08:.4;
            c.stroke.knots.clear();
            for(unsigned k=0;k<45;++k)c.stroke.knots.push_back({128.+90*std::sin(k*.42),64.+45*std::cos(k*.57),static_cast<std::uint16_t>((sample*421+k*1621)%65536)});
            std::get<CompactStrokeReferenceCommand>(entry.command).content=c;entry.record.content=c;
            const auto transform=sample%3==0?WorldToViewAffine{1.31,.2,-.17,.81,-4.73,8.42}:
                sample%3==1?WorldToViewAffine{-1.1,0,0,.8,260.3,12.7}:WorldToViewAffine{.7,0,0,1.3,11.73,-4.28};
            compare(plan(230+sample,{entry},1,transform,{0,0,256,128},{},sample%2==0));
            if(sample%6==0){
                semantic::FilledPathMask mask;
                mask.path.commands={semantic::MoveTo{{95,0}},semantic::LineTo{{145,0}},semantic::LineTo{{145,128}},semantic::LineTo{{95,128}},semantic::ClosePath{}};
                entry.record.eraseMasks={{ObjectId::fromUint64(999),mask}};
                compare(plan(300+sample,{entry},1,transform,{4,3,250,125}));
            }
        }
        auto changed=a;auto& command=std::get<CompactStrokeReferenceCommand>(changed.command);
        command.content.update([](auto& c){c.stroke.knots[1].y+=15;});changed.record.content=command.content;
        compare(plan(22,{changed}));
        renderer.clearRenderCache();require(renderer.renderCacheWork().retainedBytes==0,"clear retained bytes");
        require(provider.lose().code==BackendSubmissionCode::kAccepted,"lost");
        require(renderer.renderFrame(provider,plan(23,{a})).code!=BackendSubmissionCode::kAccepted,"lost target accepted");
        require(provider.resize(256,128).code==BackendSubmissionCode::kAccepted,"restore");
        require(reference.lose().code==BackendSubmissionCode::kAccepted&&reference.resize(256,128).code==BackendSubmissionCode::kAccepted,"reference restore");
        require(renderer.renderFrame(provider,plan(24,{a},1)).code==BackendSubmissionCode::kRejected,"stale generation presented");
        compare(plan(25,{a},provider.generation()));
        const auto beforeResource=renderer.renderCacheWork().rasterizedGroups;
        renderer.setImageResolver(nullptr);compare(plan(26,{a},provider.generation()));
        require(renderer.renderCacheWork().rasterizedGroups==beforeResource+1,"resource reset reused stale cache");
        auto huge=a;auto& hugeCommand=std::get<CompactStrokeReferenceCommand>(huge.command);
        hugeCommand.content.update([](auto& c){c.stroke.knots={{-1e7,40,32768},{1e7,40,32768}};});huge.record.content=hugeCommand.content;
        RenderCacheOptions small;small.maxEntryBytes=16;renderer.setRenderCacheOptions(small);
        compare(plan(27,{huge},provider.generation()));require(renderer.renderCacheWork().retainedBytes==0,"huge group requires giant texture");
        FailingPresent failed;require(failed.resize(256,128).code==BackendSubmissionCode::kAccepted,"failing target");
        {RenderCacheOptions options;options.gpuCoverageCandidate=gpuCandidate;renderer.setRenderCacheOptions(options);}
        require(renderer.renderFrame(failed,plan(28,{a})).code==BackendSubmissionCode::kRetry,"failed present reported accepted");
        require(renderer.renderCacheWork().retainedBytes==0,"failed present retained accepted raster state");
        failed.fail=false;require(renderer.renderFrame(failed,plan(29,{a})).code==BackendSubmissionCode::kAccepted,"present retry");
        RenderCacheOptions single;single.maxEntries=1;renderer.setRenderCacheOptions(single);
        compare(plan(30,{a,b},provider.generation()));compare(plan(31,{a,b},provider.generation()));
        require(renderer.renderCacheWork().entries==1&&renderer.renderCacheWork().evictions>0,"LRU entry limit ignored");
        const auto presents=provider.presentCount();
        auto alias=b;alias.record.objectId=a.record.objectId;
        require(renderer.renderFrame(provider,plan(32,{a,alias},provider.generation())).code==BackendSubmissionCode::kRejected,"duplicate source identity aliases cache pixels");
        require(provider.presentCount()==presents,"invalid identity presented");
        std::cout<<"unchanged canonical group reused across scene revisions\n";
        return finishNative(0);
    } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return finishNative(1);}
}
