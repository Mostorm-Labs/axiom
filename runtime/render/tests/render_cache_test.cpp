#include "canvas/render/skia_renderer.hpp"
#include "canvas/scene/scene_types.hpp"
#include <iostream>
#include <stdexcept>
#if defined(G5_TEST_WEBGL)
#include "canvas/render/webgl_surface_backend.hpp"
#include "include/core/SkSurface.h"
#include "include/core/SkCanvas.h"
#include "include/core/SkColorSpace.h"
#include "../src/compact_raster_internal.hpp"
#include <emscripten.h>
#include <emscripten/html5.h>
#endif

using namespace canvas;
using namespace canvas::render;
namespace {
void require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
#if defined(G5_TEST_WEBGL)
// Test-owned browser App. Production readback remains disabled; this reads
// pixels only outside the measured loop to compare the actual GPU targets.
class TestSurface final : public SkiaSurfaceProvider {
    std::unique_ptr<WebGlSurfaceProvider> provider_;
    std::uint64_t generation_=0,presents_=0;
    bool lost_=false;
  public:
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
            provider_=WebGlSurfaceProvider::fromCurrentContext(width,height);
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
class FailingPresent final : public SkiaSurfaceProvider {
  public:
    TestSurface raster;bool fail=true;
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
int main(int argc,char** argv) {
    const bool gpuCandidate=argc>1&&std::string_view(argv[1])=="--gpu-candidate";
    try {
        TestSurface provider;
        require(provider.resize(256,128).code==BackendSubmissionCode::kAccepted,"surface");
        SkiaRenderer renderer;

        TestSurface reference;require(reference.resize(256,128).code==BackendSubmissionCode::kAccepted,"oracle surface");
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
            auto entry=tileStroke;auto content=std::get<CompactStrokeReferenceCommand>(entry.command).content;
            content.stroke.config.family=family;content.stroke.config.materialVersion=material;
            content.stroke.config.materialResourceId=material==1?foundation::kInkMaterialResourceId:foundation::kInkMaterialV2ResourceId;
            content.stroke.config.materialResourceSha256=material==1?foundation::kInkMaterialResourceSha256:foundation::kInkMaterialV2ResourceSha256;
            entry.command=CompactStrokeReferenceCommand{content};entry.record.content=content;
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
            auto tuned=a;auto& c=std::get<CompactStrokeReferenceCommand>(tuned.command).content;
            c.stroke.config.version=2;c.stroke.config.algorithmVersion=3;c.stroke.config.signalVersion=2;
            tuned.record.content=c;
            compare(plan(590,{tuned}));
            require(renderer.renderCacheWork().gpuCoverageTiles>0,"candidate did not execute GPU coverage");
            std::cout<<"candidate GPU coverage tiles="<<renderer.renderCacheWork().gpuCoverageTiles<<'\n';
        }
#endif
        // One long source must yield within the stroke; no intermediate target
        // pixels or receipt may escape, including on camera supersession.
        {
            auto longStroke=stroke(91);
            auto& content=std::get<CompactStrokeReferenceCommand>(longStroke.command).content;
            content.stroke.config.version=2;content.stroke.config.algorithmVersion=3;content.stroke.config.signalVersion=2;
            content.stroke.config.startTaper=25;content.stroke.config.endTaper=35;
            content.stroke.knots.clear();
            for(unsigned i=0;i<240;++i)content.stroke.knots.push_back({20.+i*.85,40.+std::sin(i*.06)*15,static_cast<std::uint16_t>(12000+i*170)});
            longStroke.record.content=content;
            renderer.clearRenderCache();
            const auto previous=pixels(provider);const auto presents=provider.presentCount();
            const auto stepsBefore=renderer.renderCacheWork().preparationCoverageSteps;
            RenderPreparationBudget bounded;bounded.maxGroups=1;bounded.maxMicroseconds=1000000;bounded.maxCoverageSteps=1;
            const auto demand=plan(595,{longStroke},1,{1.2,0,0,1.2,-6.3,-2.8});
            require(renderer.renderFrame(provider,demand,nullptr,&bounded).code==BackendSubmissionCode::kRetry,
                    "single long stroke monopolizes an exact preparation opportunity");
            require(provider.presentCount()==presents&&pixels(provider)==previous,"partial stroke escaped preparation");
            require(renderer.renderCacheWork().preparationCoverageSteps==stepsBefore+1,"single-step admission exceeded");
            require(renderer.renderCacheWork().coverageTargetAllocations<=4,"temporary coverage residency unbounded");
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
            auto& editedContent=std::get<CompactStrokeReferenceCommand>(edited.command).content;
            editedContent.stroke.knots[100].y+=35;edited.record.content=editedContent;
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
        // Non-convergent residency falls back once instead of retrying forever.
        RenderCacheOptions tooSmall;tooSmall.maxEntries=1;renderer.setRenderCacheOptions(tooSmall);
        require(renderer.renderFrame(provider,preparePlan,nullptr,&slice).code==BackendSubmissionCode::kAccepted,
                "insufficient cache capacity did not fall back");
        require(direct.renderFrame(reference,preparePlan).code==BackendSubmissionCode::kAccepted&&
                pixels(provider)==pixels(reference),"capacity fallback differs from Direct");
        {RenderCacheOptions options;options.gpuCoverageCandidate=gpuCandidate;renderer.setRenderCacheOptions(options);}
        auto malformed=sources;std::get<CompactStrokeReferenceCommand>(malformed[1].command).content.stroke.config.size=-1;
        const auto invalidBefore=pixels(provider);const auto invalidPresents=provider.presentCount();
        require(renderer.renderFrame(provider,plan(603,malformed),nullptr,&slice).code==BackendSubmissionCode::kRejected,
                "invalid geometry admitted by preparation");
        require(pixels(provider)==invalidBefore&&provider.presentCount()==invalidPresents,
                "invalid preparation changed presented target");
        for(unsigned material:{1U,2U})for(auto family:{foundation::InkFamily::kRound,foundation::InkFamily::kMarker,foundation::InkFamily::kChalk,foundation::InkFamily::kCrayon,foundation::InkFamily::kWatercolor}){
            renderer.clearRenderCache();auto varied=sources;
            for(auto& entry:varied){auto content=std::get<CompactStrokeReferenceCommand>(entry.command).content;
                content.stroke.config.family=family;content.stroke.config.materialVersion=material;
                content.stroke.config.materialResourceId=material==1?foundation::kInkMaterialResourceId:foundation::kInkMaterialV2ResourceId;
                content.stroke.config.materialResourceSha256=material==1?foundation::kInkMaterialResourceSha256:foundation::kInkMaterialV2ResourceSha256;
                entry.command=CompactStrokeReferenceCommand{content};entry.record.content=content;
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
            auto wide=stroke(901);auto& wideStroke=std::get<CompactStrokeReferenceCommand>(wide.command).content.stroke;
            wideStroke.knots={{-240,50,32768},{500,50,32768}};wide.record.content=std::get<CompactStrokeReferenceCommand>(wide.command).content;
            wide.record.worldBounds={-260,30,520,70};
            auto exactFrame=plan(700,{wide});
            require(worldRenderer.renderFrame(provider,exactFrame).code==BackendSubmissionCode::kAccepted,"world exact setup");
            const auto presents=provider.presentCount(),submissions=worldRenderer.submissionCount();
            NavigationSourceQuery query=[&](const foundation::WorldRect&,std::vector<ReferenceTraversalEntry>& entries){entries={wide};return true;};
            for(unsigned n=0;n<128;++n){
                require(worldRenderer.renderNavigation(provider,{.5F,64,32},exactFrame.frame,query).code==BackendSubmissionCode::kAccepted,
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
            require(worldRenderer.renderNavigation(provider,{.5F,64,32},exactFrame.frame,query).code==BackendSubmissionCode::kAccepted&&
                    provider.presentCount()==cleanPresents,"clean navigation presented twice");
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
                require(nav.renderNavigation(retina,{1.02F,-8,-5},current,empty).code==BackendSubmissionCode::kAccepted,
                        "retina pinch fell back to synchronous exact drawing");
                if(!nav.navigationWork().pending)break;
                require(n<127,"budget-adapted navigation never converged");
            }
            require(nav.navigationWork().coverageComplete&&retina.presentCount()>0&&nav.submissionCount()==submissions,
                    "budget-adapted feedback missing or qualified canonical");
            require(nav.navigationWork().retainedBytes<=RenderCacheOptions{}.maxBytes,"retina navigation exceeded budget");
        }
        {
            TestSurface feedback;require(feedback.resize(256,128).code==BackendSubmissionCode::kAccepted,"cold feedback target");
            SkiaRenderer nav;RenderCacheOptions options;options.navigationMaxSteps=1;nav.setRenderCacheOptions(options);
            auto entry=stroke(908);auto exact=plan(709,{entry});
            require(nav.renderFrame(feedback,exact).code==BackendSubmissionCode::kAccepted,"cold feedback exact");
            const auto before=pixels(feedback);
            const auto presents=feedback.presentCount(),submissions=nav.submissionCount();
            NavigationSourceQuery query=[&](const foundation::WorldRect&,std::vector<ReferenceTraversalEntry>& e){e={entry};return true;};
            require(nav.renderNavigation(feedback,{1.2F,-25.6F,-12.8F},exact.frame,query).code==BackendSubmissionCode::kAccepted,
                    "cold pinch feedback rejected");
            require(nav.navigationWork().pending&&nav.navigationWork().presented&&feedback.presentCount()==presents+1,
                    "cold pinch froze while world tiles were preparing");
            require(pixels(feedback)!=before&&nav.submissionCount()==submissions,"cold feedback did not move pixels or qualified canonical");
            // A viewport screenshot may supply feedback only inside its known
            // coverage. Zoom-out must keep the complete display until world
            // coverage exists, never bring back the rectangular crop.
            const auto prior=feedback.presentCount();
            require(nav.renderNavigation(feedback,{.5F,64,32},exact.frame,query).code==BackendSubmissionCode::kAccepted,
                    "cold zoom-out rejected");
            require(!nav.navigationWork().coverageComplete&&feedback.presentCount()==prior,
                    "partial screenshot was stretched over newly exposed content");
        }
        {
            TestSurface longSurface;require(longSurface.resize(256,128).code==BackendSubmissionCode::kAccepted,"long target");
            SkiaRenderer nav;RenderCacheOptions bounded;bounded.navigationMaxSteps=8;nav.setRenderCacheOptions(bounded);
            auto longEntry=stroke(910);auto& content=std::get<CompactStrokeReferenceCommand>(longEntry.command).content;
            content.stroke.knots.clear();
            for(unsigned k=0;k<4096;++k)content.stroke.knots.push_back({-100.+k*.1,64,32768});
            longEntry.record.content=content;longEntry.record.worldBounds={-130,35,340,93};
            auto current=plan(710,{longEntry});
            NavigationSourceQuery query=[&](const foundation::WorldRect&,std::vector<ReferenceTraversalEntry>& e){e={longEntry};return true;};
            require(nav.renderNavigation(longSurface,{1,0,0},current.frame,query).code==BackendSubmissionCode::kAccepted,"long navigation");
            require(nav.navigationWork().pending&&nav.navigationWork().knotSteps<=256,
                    "long stroke did not yield inside coverage");
            const auto beforeKnot=nav.navigationWork().knotSteps;
            // Slight camera motion does not obsolete a world-addressed job.
            require(nav.renderNavigation(longSurface,{1,-.1F,0},current.frame,query).code==BackendSubmissionCode::kAccepted,
                    "world job camera update");
            require(nav.navigationWork().knotSteps>beforeKnot,"camera update starved world job");
            // A camera change cancels its partially rasterized tile; old work
            // must not publish after target loss or a resource epoch change.
            require(longSurface.lose().code==BackendSubmissionCode::kAccepted,"nav lost");
            require(nav.renderNavigation(longSurface,{.5F,0,0},current.frame,query).code!=BackendSubmissionCode::kAccepted,
                    "lost navigation accepted");
            require(longSurface.resize(256,128).code==BackendSubmissionCode::kAccepted,"nav restore");
            require(nav.renderNavigation(longSurface,{1,0,0},current.frame,query).code==BackendSubmissionCode::kRejected,
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
                require(nav.renderNavigation(provider,{1,0,0},current.frame,query).code==BackendSubmissionCode::kAccepted,"vector navigation");
                if(!nav.navigationWork().pending)break;require(n<127,"vector nav loop");
            }
            const auto image=pixels(provider);
            for(unsigned x:{1U,63U,127U,128U,129U,254U})
                require(image[(60*256+x)*4]>=126&&image[(60*256+x)*4]<=129,"gutter or fallback double-blended vector ink");
            const auto built=nav.navigationWork().tilesBuilt;
            // Pan exposes at most one new column; interior world rasters stay reusable.
            require(nav.renderNavigation(provider,{1,-1,0},current.frame,query).code==BackendSubmissionCode::kAccepted,"world reuse pan");
            require(nav.navigationWork().tilesBuilt<=built+2,"camera pan rebuilt all world rasters");
            nav.setImageResolver(nullptr);
            require(nav.renderNavigation(provider,{1,-1,0},current.frame,query).code==BackendSubmissionCode::kAccepted&&
                    nav.navigationWork().tilesBuilt<=(built+8),"resource reset unusable");
        }
        {
            SkiaRenderer nav;auto entry=stroke(950);auto frame=plan(730,{entry});
            NavigationSourceQuery query=[&](const foundation::WorldRect&,std::vector<ReferenceTraversalEntry>& e){e={entry};return true;};
            for(unsigned n=0;n<128;++n){
                require(nav.renderNavigation(provider,{1,-10,0},frame.frame,query).code==BackendSubmissionCode::kAccepted,"parent setup");
                if(!nav.navigationWork().pending)break;require(n<127,"parent setup loop");
            }
            RenderCacheOptions singleStep;singleStep.navigationMaxSteps=1;nav.setRenderCacheOptions(singleStep);
            // Rebuild the base plane with deterministic one-quantum admission.
            for(unsigned n=0;n<256;++n){
                require(nav.renderNavigation(provider,{1,-10,0},frame.frame,query).code==BackendSubmissionCode::kAccepted,"fallback base");
                if(!nav.navigationWork().pending)break;require(n<255,"fallback base loop");
            }
            const auto fallbackBefore=nav.navigationWork().fallbackUses;
            require(nav.renderNavigation(provider,{1.5F,-12,0},frame.frame,query).code==BackendSubmissionCode::kAccepted,
                    "parent scale switch");
            require(nav.navigationWork().fallbackUses>fallbackBefore,"ready parent coverage not selected");
            // Changed sources in the same world cells invalidate retained pixels.
            auto& config=std::get<CompactStrokeReferenceCommand>(entry.command).content.stroke.config;
            config.color={1,0,0,1};entry.record.content=std::get<CompactStrokeReferenceCommand>(entry.command).content;
            auto next=plan(731,{entry});
            for(unsigned n=0;n<128;++n){
                require(nav.renderNavigation(provider,{1,0,0},next.frame,query).code==BackendSubmissionCode::kAccepted,"new content nav");
                if(!nav.navigationWork().pending)break;require(n<127,"new content loop");
            }
            const auto red=pixels(provider);require(red[(50*256+80)*4]>red[(50*256+80)*4+2],"stale world content survived scene change");
            RenderCacheOptions tiny;tiny.maxEntries=1;nav.setRenderCacheOptions(tiny);
            const auto presents=provider.presentCount();
            require(nav.renderNavigation(provider,{1,0,0},next.frame,query).code!=BackendSubmissionCode::kAccepted&&
                    provider.presentCount()==presents,"nonconvergent residency published navigation");
        }
        {
            SkiaRenderer nav;
            auto entry=stroke(960);auto invalid=plan(740,{entry});
            NavigationSourceQuery malformed=[&](const foundation::WorldRect&,std::vector<ReferenceTraversalEntry>& e){
                e={entry,entry};return true;
            };
            const auto presents=provider.presentCount();
            require(nav.renderNavigation(provider,{1,0,0},invalid.frame,malformed).code!=BackendSubmissionCode::kAccepted&&
                    provider.presentCount()==presents,"duplicate world source double-painted");
        }
        // Navigation feedback is a separate submit and cannot manufacture a
        // canonical frame identity. Exact rendering after it must match Direct.
        compare(plan(90,{a}));
        const auto exactRaster=renderer.rasterizationCount();
        const auto oldPresent=provider.presentCount();
        const auto beforeNavigation=pixels(provider);
        require(renderer.renderNavigation(provider,{1.1F,4.F,-3.F},
            plan(90,{a}).frame).code==BackendSubmissionCode::kAccepted,
            "navigation reprojection unavailable");
        require(provider.presentCount()==oldPresent+1 && renderer.rasterizationCount()==exactRaster,
            "navigation rebuilt canonical stroke geometry");
        require(pixels(provider)!=beforeNavigation,"navigation present did not move visible pixels");
        auto stale=plan(91,{a});
        require(renderer.renderNavigation(provider,{1.1F,4.F,-3.F},stale.frame).code!=BackendSubmissionCode::kAccepted,
            "navigation reused another scene revision");
        compare(plan(92,{a},1,{1.1,0,0,1.1,4,-3}));
        renderer.clearRenderCache();

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
            auto entry=a;auto content=std::get<CompactStrokeReferenceCommand>(entry.command).content;
            content.stroke.config.family=family;content.stroke.config.materialVersion=material;
            content.stroke.config.materialResourceId=material==1?foundation::kInkMaterialResourceId:foundation::kInkMaterialV2ResourceId;
            content.stroke.config.materialResourceSha256=material==1?foundation::kInkMaterialResourceSha256:foundation::kInkMaterialV2ResourceSha256;
            entry.command=CompactStrokeReferenceCommand{content};entry.record.content=content;
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
            auto& c=std::get<CompactStrokeReferenceCommand>(entry.command).content;
            c.stroke.config.version=2;c.stroke.config.algorithmVersion=3;c.stroke.config.signalVersion=2;
            c.stroke.config.family=family;c.stroke.config.startCap=caps;c.stroke.config.endCap=caps;
            c.stroke.config.startTaper=caps?40:0;c.stroke.config.endTaper=caps?70:0;
            c.stroke.knots.clear();
            for(unsigned i=0;i<40;++i)c.stroke.knots.push_back({18.+i*5,55.+std::sin(i*.7)*14,static_cast<std::uint16_t>(7000+i*1300)});
            entry.record.content=c;
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
            auto& c=std::get<CompactStrokeReferenceCommand>(entry.command).content;
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
            entry.record.content=c;
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
        command.content.stroke.knots[1].y+=15;changed.record.content=command.content;
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
        hugeCommand.content.stroke.knots={{-1e7,40,32768},{1e7,40,32768}};huge.record.content=hugeCommand.content;
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
        return 0;
    } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
