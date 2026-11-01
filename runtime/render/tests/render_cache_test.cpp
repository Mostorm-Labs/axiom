#include "canvas/render/skia_renderer.hpp"
#include "canvas/scene/scene_types.hpp"
#include <iostream>
#include <stdexcept>
#if defined(G5_TEST_WEBGL)
#include "canvas/render/webgl_surface_backend.hpp"
#include "include/core/SkSurface.h"
#include "include/core/SkColorSpace.h"
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
               foundation::DocumentBackground background = {}, bool transparent=false) {
    FrameState frame{ViewId{1}, CameraState{{0,0},1,0,CameraGeneration{1}},
        {0,0,256,128}, {256,128,256,128,1,1}, semantic::SemanticGeneration{frameId},
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
int main() {
    try {
        TestSurface provider;
        require(provider.resize(256,128).code==BackendSubmissionCode::kAccepted,"surface");
        SkiaRenderer renderer;
        TestSurface reference;require(reference.resize(256,128).code==BackendSubmissionCode::kAccepted,"oracle surface");
        SkiaRenderer direct;RenderCacheOptions disabled;disabled.enabled=false;direct.setRenderCacheOptions(disabled);
        auto compare=[&](const FramePlan& frame){
            require(renderer.renderFrame(provider,frame).code==BackendSubmissionCode::kAccepted,"cached frame");
            require(direct.renderFrame(reference,frame).code==BackendSubmissionCode::kAccepted,"direct frame");
            const auto actual=pixels(provider),expected=pixels(reference);
            if(actual!=expected){unsigned differences=0,maxError=0;for(std::size_t i=0;i<actual.size();++i){differences+=actual[i]!=expected[i];maxError=std::max(maxError,unsigned(std::abs(int(actual[i])-int(expected[i]))));}
                std::cerr<<"frame="<<frame.frame.frameId.value()<<" different channels="<<differences<<" max="<<maxError<<'\n';}
            require(actual==expected,"cached/direct pixels differ");
        };
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
        renderer.setRenderCacheOptions({});
        const auto a=stroke(1);
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
        renderer.setRenderCacheOptions({});
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
        renderer.setRenderCacheOptions({});
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
