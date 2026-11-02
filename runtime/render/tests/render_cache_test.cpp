#include "canvas/render/skia_renderer.hpp"
#include "canvas/scene/scene_types.hpp"
#include "canvas/ink/brush_package_catalog.hpp"
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
            // Content, order, camera, target and resource changes must never
            // reuse old pixels, including remove/readd with a repeated ID.
            auto content=std::get<CompactStrokeReferenceCommand>(sources[0].command).content;
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
                auto large=stroke(9710);auto content=std::get<CompactStrokeReferenceCommand>(large.command).content;
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
                auto changed=b;auto material=std::get<CompactStrokeReferenceCommand>(changed.command).content;
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
                auto content=std::get<CompactStrokeReferenceCommand>(entry.command).content;
                content.stroke.config=*ink::BrushPackageCatalog{}.loadDefault(profile,revision).package.compactConfig;
                content.stroke.config.size=18;content.stroke.config.opacity=.55;content.stroke.seed=19377;
                content.stroke.knots.clear();
                for(unsigned k=0;k<100;++k)content.stroke.knots.push_back({15.+k*2.1,64.+std::sin(k*.16)*23,static_cast<std::uint16_t>(18000+k*300)});
                entry.command=CompactStrokeReferenceCommand{content};entry.record.content=content;
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
            auto& content=std::get<CompactStrokeReferenceCommand>(longStroke.command).content;
            content.stroke.config.version=2;content.stroke.config.algorithmVersion=3;content.stroke.config.signalVersion=2;
            content.stroke.config.startTaper=25;content.stroke.config.endTaper=35;
            content.stroke.knots.clear();
            for(unsigned i=0;i<240;++i)content.stroke.knots.push_back({20.+i*.85,40.+std::sin(i*.06)*15,static_cast<std::uint16_t>(12000+i*170)});
            longStroke.record.content=content;
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
            auto& longStroke = std::get<CompactStrokeReferenceCommand>(longEntry.command).content.stroke;
            longStroke.knots = {{-120, 58, 32768}, {420, 58, 32768}};
            longEntry.record.content = std::get<CompactStrokeReferenceCommand>(longEntry.command).content;
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
            auto longEntry=stroke(910);auto& content=std::get<CompactStrokeReferenceCommand>(longEntry.command).content;
            content.stroke.knots.clear();
            for(unsigned k=0;k<4096;++k)content.stroke.knots.push_back({-100.+k*.1,64,32768});
            longEntry.record.content=content;longEntry.record.worldBounds={-130,35,340,93};
            auto current=plan(710,{longEntry});
            NavigationSourceQuery query=[&](const foundation::WorldRect&,std::vector<ReferenceTraversalEntry>& e){e={longEntry};return true;};
            require(navigate(nav,longSurface,{1,0,0},current.frame,query).code==BackendSubmissionCode::kAccepted,"long navigation");
            require(nav.navigationWork().pending&&nav.navigationWork().knotSteps<=256,
                    "long stroke did not yield inside coverage");
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
            auto& config=std::get<CompactStrokeReferenceCommand>(entry.command).content.stroke.config;
            config.color={1,0,0,1};entry.record.content=std::get<CompactStrokeReferenceCommand>(entry.command).content;
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
