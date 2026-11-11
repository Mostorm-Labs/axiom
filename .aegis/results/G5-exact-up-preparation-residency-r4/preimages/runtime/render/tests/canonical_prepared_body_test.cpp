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
using namespace canvas::render::internal;
namespace {
void require(bool v,const char* m){if(!v)throw std::runtime_error(m);}
ReferenceTraversalEntry source(){
    semantic::CompactStrokeContent c;c.stroke.config.version=2;c.stroke.config.algorithmVersion=4;
    c.stroke.config.signalVersion=3;c.stroke.config.size=18;c.stroke.config.smoothing=.5;c.stroke.seed=42;
    for(unsigned i=0;i<7199;++i)c.stroke.knots.push_back({i*.2,64+25*std::sin(i*.023),std::uint16_t(22000+(i%100)*200)});
    ReferenceTraversalEntry e;e.record.objectId=ObjectId::fromUint64(1);e.record.content=std::move(c);
    e.command=CompactStrokeReferenceCommand{scene::ImmutableValue<semantic::CompactStrokeContent>::alias(e.record.content)};
    e.record.referenceGeometryDigest="deliberately-not-an-authority";return e;
}
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
    std::shared_ptr<GpuCompletionService> gpuCompletionService()const noexcept override{return completion_;}
    RenderTargetInfo describe()const noexcept override{RenderTargetInfo info;info.profileId="native-test-metal";info.kind=RenderTargetKind::kGpuOffscreen;info.backend=RenderTargetBackend::kMetal;info.capabilities.gpuAccelerated=true;info.capabilities.supportsReadback=true;info.capabilities.supportsPresent=true;info.capabilities.supportsPartialUpdate=true;return info;}
    SkiaSurfaceAcquireResult acquire()noexcept override{return surface_?SkiaSurfaceAcquireResult::acquired({surface_.get(),generation_}):SkiaSurfaceAcquireResult{};}
    void release()noexcept override{}
    BackendSubmissionResult present()noexcept override{completion_->context->flushAndSubmit(GrSyncCpu::kYes);++presents_;return BackendSubmissionResult::accepted();}
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
#endif


void boundedService(){
    auto budget=std::make_shared<resources::ResourceBudgetManager>();PreparedGeometryCache cache;RenderCacheOptions options;
    auto a=source();auto body=cache.request(a,options,budget);require(body&&!body->ready(),"cold request synchronously prepared history");
    const auto initial=cache.statistics();require(initial.supportKnots==0&&initial.compileInputs==0,"hidden request scans");
    auto equal=a;equal.record.content=semantic::ObjectContent{a.record.content.get()};equal.command=CompactStrokeReferenceCommand{scene::ImmutableValue<semantic::CompactStrokeContent>::alias(equal.record.content)};
    require(cache.request(equal,options,budget)==body,"equal pending allocation did not share job");
    auto fork=a;fork.record.content.update([](auto& c){++std::get<semantic::CompactStrokeContent>(c).stroke.seed;});
    fork.command=CompactStrokeReferenceCommand{scene::ImmutableValue<semantic::CompactStrokeContent>::alias(fork.record.content)};
    auto forkBody=cache.request(fork,options,budget);require(forkBody&&forkBody!=body,"pending config/seed fork reused source");
    require(!cache.get(a,options,budget)&&!cache.lookup(a,PreparedConsumer::kDirect),"compatibility get drained pending job");
    const auto before=cache.statistics();require(before.compileInputs==0&&before.supportKnots==0,"compatibility path did work");
    RenderPreparationBudget one;one.maxCoverageSteps=1;
    unsigned steps=0;
    while(!body->ready()){
        PreparationAllowance allowance(one);const auto previous=cache.statistics();cache.advance(body,allowance);const auto current=cache.statistics();
        require(allowance.steps==1&&current.supportKnots-previous.supportKnots<=32&&current.distanceInputs-previous.distanceInputs<=32&&current.compileInputs-previous.compileInputs<=32,"one step exceeded shared source quantum");
        require(current.pathPrimitives-previous.pathPrimitives<=32,"path/group build exceeded quantum");
        require(++steps<2048,"stable body failed to progress");
    }
    const auto ready=cache.statistics();require(ready.builds==1&&ready.supportKnots==7199&&ready.distanceInputs==7198&&ready.compileInputs==7199&&ready.maxAdvanceKnots==32,"unique body preparation not exactly once");
    require(cache.lookup(equal,PreparedConsumer::kCanonical)==body,"equal pending alias failed ready lookup");
    auto cpu=SkSurfaces::Raster(SkImageInfo::MakeN32Premul(256,128)),oracle=SkSurfaces::Raster(SkImageInfo::MakeN32Premul(256,128));
    const auto& stroke=std::get<CompactStrokeReferenceCommand>(a.command).content.get().stroke;
    for(auto* surface:{cpu.get(),oracle.get()})surface->getCanvas()->clear(SK_ColorBLACK);
    body->drawContour(*cpu->getCanvas());drawCompactCoverageToSkCanvas(*oracle->getCanvas(),stroke.config,stroke.knots);
    std::vector<std::uint8_t> actual(256*128*4),expected(actual.size());const auto info=SkImageInfo::Make(256,128,kRGBA_8888_SkColorType,kPremul_SkAlphaType);
    require(cpu->readPixels(info,actual.data(),info.minRowBytes(),0,0)&&oracle->readPixels(info,expected.data(),info.minRowBytes(),0,0)&&actual==expected,"incremental paths differ from independent full-history CPU geometry");
    auto pendingFork=forkBody;cache.clear();require(budget->used()>0,"held ready/pending leases were uncharged");
    const auto cleared=cache.statistics();PreparationAllowance stale(one);cache.advance(pendingFork,stale);
    require(cache.statistics().advances==cleared.advances&&stale.steps==0,"cancelled cache job advanced after removal");
    body.reset();forkBody.reset();pendingFork.reset();require(budget->used()==0,"last ready/pending source/index release leaked");
    auto held=budget->reserve(resources::ResourceClass::kScratch,96U*1024U*1024U,true);require(bool(held),"quota pressure");
    require(!cache.request(a,options,budget),"over-cap body admitted");budget->release(*held);
    require(bool(cache.request(a,options,budget)),"quota release did not recover");cache.clear();require(budget->used()==0,"failed admission metadata leaked");
    auto replayBudget=std::make_shared<resources::ResourceBudgetManager>(resources::BudgetLimits{2U*1024U*1024U,4U*1024U*1024U});
    auto pressure=replayBudget->reserve(resources::ResourceClass::kTile,1024U*1024U,true);require(bool(pressure),"replay pressure");
    PreparedGeometryCache replayCache;auto replay=replayCache.request(a,options,replayBudget);require(bool(replay),"bounded checkpoint fallback admission");
    while(!replay->ready()){PreparationAllowance allowance(one);replayCache.advance(replay,allowance);require(!replay->failed(),"replay build failed");}
    cpu->getCanvas()->clear(SK_ColorBLACK);const auto replayWork=replay->drawContour(*cpu->getCanvas());
    require(replayWork.compiledKnots>0,"checkpoint mode hid actual replay compilation");
    require(cpu->readPixels(info,actual.data(),info.minRowBytes(),0,0)&&actual==expected,"bounded checkpoint replay differs from independent CPU contour");
    std::printf("{\"visible_checkpoint_replay_inputs\":%llu,\"maximum_build_quantum\":%llu}\n",(unsigned long long)replayWork.compiledKnots,(unsigned long long)replayCache.statistics().maxAdvanceKnots);
    replayCache.clear();replay.reset();replayBudget->release(*pressure);require(replayBudget->used()==0,"checkpoint last-release leak");
    options.maxEntryBytes=256;require(!cache.request(a,options,budget)&&cache.requestRejected(),"structurally impossible body must reject rather than retry forever");
    std::printf("{\"bounded_service\":true,\"advances\":%u,\"unique_support\":7199,\"unique_length\":7198,\"unique_compile\":7199,\"maximum_advance_knots\":32}\n",steps);
}

std::vector<bool> independentEdges(const ReferenceTraversalEntry& entry,unsigned dpr){
    const unsigned width=256*dpr,height=128*dpr;std::vector<bool> edge(width*height,false);
    for(bool masks:{false,true}){
        auto cpu=SkSurfaces::Raster(SkImageInfo::MakeN32Premul(width,height));auto* canvas=cpu->getCanvas();canvas->clear(masks?SK_ColorWHITE:SK_ColorBLACK);canvas->scale(dpr,dpr);
        const auto& t=entry.record.transform;SkMatrix matrix;matrix.setAll(float(t.a),float(t.c),float(t.tx),float(t.b),float(t.d),float(t.ty),0,0,1);canvas->concat(matrix);
        if(masks)drawEraseMasksToSkCanvas(*canvas,entry);
        else {const auto& stroke=std::get<CompactStrokeReferenceCommand>(entry.command).content.get().stroke;drawCompactCoverageToSkCanvas(*canvas,stroke.config,stroke.knots);}
        std::vector<std::uint8_t> bytes(width*height*4);auto info=SkImageInfo::Make(width,height,kRGBA_8888_SkColorType,kPremul_SkAlphaType);require(cpu->readPixels(info,bytes.data(),info.minRowBytes(),0,0),"independent CPU AA geometry");
        for(unsigned y=0;y<height;++y)for(unsigned x=0;x<width;++x){const auto value=bytes[(y*width+x)*4+(masks?3:0)];
            if(value>0&&value<255)for(int dy=-2;dy<=2;++dy)for(int dx=-2;dx<=2;++dx){const int nx=int(x)+dx,ny=int(y)+dy;if(nx>=0&&ny>=0&&nx<int(width)&&ny<int(height))edge[ny*width+nx]=true;}}
    }
    return edge;
}

void boundedGeometryVariants(){
    for(unsigned variant:{0U,1U,2U,3U}){
        auto e=source();auto c=std::get<CompactStrokeReferenceCommand>(e.command).content.get();
        if(variant==0){c.stroke.config.startTaper=70;c.stroke.config.endTaper=90;}
        if(variant==1){c.stroke.config.startCap=false;c.stroke.config.endCap=false;}
        if(variant==2)for(std::size_t i=0;i<c.stroke.knots.size();++i){auto& k=c.stroke.knots[i];k.x=128+85*std::sin(i*.031);k.y=64+42*std::sin(i*.049);}
        if(variant==3){c.stroke.config.family=foundation::InkFamily::kMarker;c.stroke.config.markerAngle=.3;c.stroke.config.markerFlatness=.6;}
        e.record.content=c;e.command=CompactStrokeReferenceCommand{scene::ImmutableValue<semantic::CompactStrokeContent>::alias(e.record.content)};
        PreparedGeometryCache cache;auto budget=std::make_shared<resources::ResourceBudgetManager>();RenderCacheOptions options;auto body=cache.request(e,options,budget);require(bool(body),"variant body admission");
        while(!body->ready()){PreparationAllowance allowance;cache.advance(body,allowance);require(!body->failed(),"variant body preparation");}
        auto actual=SkSurfaces::Raster(SkImageInfo::MakeN32Premul(256,128)),expected=SkSurfaces::Raster(SkImageInfo::MakeN32Premul(256,128));
        for(auto* surface:{actual.get(),expected.get()})surface->getCanvas()->clear(SK_ColorBLACK);
        body->drawContour(*actual->getCanvas());drawCompactCoverageToSkCanvas(*expected->getCanvas(),c.stroke.config,c.stroke.knots);
        std::vector<std::uint8_t> a(256*128*4),b(a.size());auto info=SkImageInfo::Make(256,128,kRGBA_8888_SkColorType,kPremul_SkAlphaType);
        require(actual->readPixels(info,a.data(),info.minRowBytes(),0,0)&&expected->readPixels(info,b.data(),info.minRowBytes(),0,0)&&a==b,"incremental caps/tapers/self-intersection/nib geometry changed independent CPU pixels");
        cache.clear();body.reset();require(budget->used()==0,"variant leases leaked");
    }
    std::printf("{\"independent_cpu_geometry_variants\":[\"tapers\",\"caps-disabled\",\"self-intersection\",\"marker-nib\"],\"pixel_parity\":true}\n");
}
#if defined(__APPLE__)
void realOneStepAndPixels(){
    auto pool=message(message(reinterpret_cast<id>(objc_getClass("NSAutoreleasePool")),"alloc"),"init");
    {
        const auto device=MTLCreateSystemDefaultDevice();const auto queue=message(device,"newCommandQueue");
        auto completion=std::make_shared<NativeMetalCompletion>();GrMtlBackendContext backend{sk_cfp<GrMTLHandle>(device),sk_cfp<GrMTLHandle>(queue)};completion->context=GrDirectContexts::MakeMetal(backend);
        require(bool(completion->context),"pixel Metal context");
        for(unsigned dpr:{1U,2U,4U})for(unsigned variant:{0U,1U,2U}){
            auto e=source();semantic::ObjectRecord object;object.id=e.record.objectId;object.kind=semantic::ObjectKind::kVectorStroke;object.kind_version=3;
            object.content=e.record.content.get();std::get<semantic::CompactStrokeContent>(object.content).stroke.config.opacity=.55;
            if(variant)object.transform={.97,.1,-.1,.97,6,-5};
            if(variant==2){semantic::SweptCircleMask m;m.segments.push_back({{{65,20},8},{{65,105},8},{65,40},{65,80}});object.erase_masks={{{},m}};}
            e.record=projectRuntimeScene({&object,1},semantic::SemanticGeneration{1}).records.front();e.command=CompactStrokeReferenceCommand{scene::ImmutableValue<semantic::CompactStrokeContent>::alias(e.record.content)};
            NativeMetalTarget target(completion),reference(completion);require(target.resize(256*dpr,128*dpr).code==BackendSubmissionCode::kAccepted&&reference.resize(256*dpr,128*dpr).code==BackendSubmissionCode::kAccepted,"pixel target resize");
            const auto acquired=target.acquire();acquired.frame.surface->getCanvas()->clear(SK_ColorMAGENTA);target.release();
            std::vector<std::uint8_t> initial(256*128*dpr*dpr*4),actual(initial.size()),expected(initial.size());require(target.readbackRgba(initial).code==BackendSubmissionCode::kAccepted,"initial pixels");
            SkiaRenderer candidate;auto plan=renderPlan(e,1,dpr);RenderPreparationBudget slice;slice.maxCoverageSteps=1;unsigned attempts=0;
            for(;;){const auto result=candidate.renderFrame(target,plan,nullptr,&slice);++attempts;if(result.code==BackendSubmissionCode::kAccepted)break;
                require(result.code==BackendSubmissionCode::kRetry&&attempts<8192,"one-step canonical failed to converge");
                require(target.presentCount()==0,"one-step pending presented");
                if(attempts==1){require(target.readbackRgba(actual).code==BackendSubmissionCode::kAccepted&&actual==initial,"pending wrote final provider");}
                if(attempts==9){auto cameraDraw=plan.referenceDrawList;cameraDraw.worldToView.tx+=1;
                    FramePlan camera{plan.frame,std::move(cameraDraw),{},false,true};const auto before=candidate.renderCacheWork().preparedSupportKnots;
                    require(candidate.renderFrame(target,camera,nullptr,&slice).code==BackendSubmissionCode::kRetry,"camera pending unexpectedly presented");
                    require(candidate.renderCacheWork().preparedSupportKnots>before,"camera reset world-body progress");}
            }
            require(candidate.renderCacheWork().preparedBodyBuilds==1&&candidate.renderCacheWork().preparedCanonicalDraws>0,"actual canonical raster job bypassed ready body");
            SkiaRenderer independent;RenderCacheOptions disabled;disabled.enabled=false;independent.setRenderCacheOptions(disabled);
            require(independent.renderFrame(reference,plan).code==BackendSubmissionCode::kAccepted,"independent GPU null-cache reference");
            require(target.readbackRgba(actual).code==BackendSubmissionCode::kAccepted&&reference.readbackRgba(expected).code==BackendSubmissionCode::kAccepted,"final pixels");
            if(actual!=expected){std::size_t changed=0;unsigned maximum=0;for(std::size_t i=0;i<actual.size();++i)if(actual[i]!=expected[i]){++changed;maximum=std::max(maximum,unsigned(std::abs(int(actual[i])-int(expected[i]))));}
                const auto prefix=std::string("out/G5-UR1c2-working-set-repair-r1/native-pixel-")+std::to_string(dpr)+"-"+std::to_string(variant);
                std::ofstream(prefix+"-actual.rgba",std::ios::binary).write(reinterpret_cast<const char*>(actual.data()),actual.size());std::ofstream(prefix+"-reference.rgba",std::ios::binary).write(reinterpret_cast<const char*>(expected.data()),expected.size());
                std::fprintf(stderr,"pixel mismatch dpr=%u variant=%u channels=%zu max=%u\n",dpr,variant,changed,maximum);
            }
            const auto edges=independentEdges(e,dpr);std::size_t interiorFailures=0;
            for(std::size_t i=0;i<actual.size();++i)if(!edges[i/4]&&std::abs(int(actual[i])-int(expected[i]))>2)++interiorFailures;
            require(interiorFailures==0,"prepared canonical changed non-AA GPU mask/transform/opacity/z pixels");
            const auto before=candidate.renderCacheWork();EditingOverlay overlay{plan.frame};
            require(candidate.renderTransientOverlay(target,plan,&overlay).code==BackendSubmissionCode::kAccepted,"warm full transient");
            require(candidate.renderCacheWork().preparedBodyBuilds==before.preparedBodyBuilds,"warm Direct rebuilt body");
            auto stale=renderPlan(e,2,dpr,2);const auto presents=target.presentCount();
            require(candidate.renderFrame(target,stale,nullptr,&slice).code==BackendSubmissionCode::kRejected&&target.presentCount()==presents,"stale surface presented");
            std::printf("{\"native_gpu_pixel_parity\":true,\"dpr\":%u,\"variant\":%u,\"one_step_attempts\":%u}\n",dpr,variant,attempts);
        }
    }
    message(pool,"drain");
}
#endif

#if defined(__APPLE__)
// Same independent two-device-pixel per-primitive boundary as the frozen
// ab7a1a3 CPU oracle; no candidate-derived difference mask or larger tolerance.
std::vector<bool> workingSetEdges(const ReferenceTraversalEntry& e,unsigned dpr){
    auto edge=independentEdges(e,dpr);const unsigned width=256*dpr,height=128*dpr;
    auto surface=SkSurfaces::Raster(SkImageInfo::MakeA8(width,height));surface->getCanvas()->clear(SK_ColorTRANSPARENT);
    const auto& t=e.record.transform;auto matrix=SkMatrix::Scale(dpr,dpr);
    matrix.preConcat(SkMatrix::MakeAll(t.a,t.c,t.tx,t.b,t.d,t.ty,0,0,1));
    SkPaint boundary;boundary.setColor(SK_ColorWHITE);boundary.setStyle(SkPaint::kStroke_Style);boundary.setStrokeWidth(2);boundary.setAntiAlias(true);
    const auto draw=[&](const SkPath& path){surface->getCanvas()->drawPath(path.makeTransform(matrix),boundary);};
    const auto& stroke=std::get<CompactStrokeReferenceCommand>(e.command).content.get().stroke;
    const auto contour=ink::compileCompactContour(stroke.config,stroke.knots);
    for(std::size_t i=0;i+1<contour.patches.size();++i)draw(compactContourRibbon(std::span(contour.patches).subspan(i,2)));
    for(const auto& cap:contour.caps){SkPathBuilder path;path.addCircle(float(cap.x),float(cap.y),float(cap.radius));draw(path.detach());}
    std::vector<std::uint8_t> geometric(edge.size());require(surface->readPixels(SkImageInfo::MakeA8(width,height),geometric.data(),width,0,0),"independent primitive boundary");
    if(stroke.knots.size()==7199&&dpr==4)std::ofstream("out/G5-UR1c2-working-set-repair-r1/independent-boundary.a8",std::ios::binary).write(reinterpret_cast<const char*>(geometric.data()),geometric.size());
    for(std::size_t i=0;i<edge.size();++i)edge[i]=edge[i]||geometric[i];return edge;
}
FramePlan workingSetPlan(unsigned count,bool equalAllocations,unsigned dpr){
    auto c=std::get<CompactStrokeReferenceCommand>(source().command).content.get();
    std::vector<semantic::ObjectRecord> objects;
    for(unsigned n=0;n<count;++n){
        semantic::ObjectRecord o;o.id=ObjectId::fromUint64(n+1);o.kind=semantic::ObjectKind::kVectorStroke;o.kind_version=3;
        auto value=c;if(n||equalAllocations)value.stroke.knots.resize(64);
        if(!equalAllocations)value.stroke.seed+=n;
        o.content=std::move(value);o.placement.order_key=semantic::OrderKey({std::uint8_t(n/250+1),std::uint8_t(n%250+1)});
        objects.push_back(std::move(o));
    }
    const auto projected=projectRuntimeScene(objects,semantic::SemanticGeneration{1});
    auto f=renderPlan(source(),1,dpr).frame;
    ReferenceDrawList draw{f};draw.viewportClip={0,0,256,128};
    for(const auto& record:projected.records)draw.entries.push_back({record,true,CompactStrokeReferenceCommand{scene::ImmutableValue<semantic::CompactStrokeContent>::alias(record.content)}});
    std::stable_partition(draw.entries.begin(),draw.entries.end(),[](const auto& e){return e.record.objectId==ObjectId::fromUint64(1);});
    return FramePlan{f,std::move(draw),{},false,true};
}

void heldWorkingSet(){
    auto budget=std::make_shared<resources::ResourceBudgetManager>(resources::BudgetLimits{32U*1024U*1024U,96U*1024U*1024U});PreparedGeometryCache cache;RenderCacheOptions options;
    auto longSource=source();auto pending=cache.request(longSource,options,budget);require(pending&&!pending->ready(),"held current job admission");
    std::vector<std::shared_ptr<const PreparedNavigationGeometry>> held;
    for(unsigned n=0;n<513;++n){auto e=source();auto c=std::get<CompactStrokeReferenceCommand>(e.command).content.get();c.stroke.knots.resize(64);c.stroke.seed+=n+1;
        e.record.content=std::move(c);e.command=CompactStrokeReferenceCommand{scene::ImmutableValue<semantic::CompactStrokeContent>::alias(e.record.content)};
        auto body=cache.request(e,options,budget);require(bool(body),"held ready body admission");
        while(!body->ready()){PreparationAllowance allowance;cache.advance(body,allowance);require(!body->failed(),"held ready body compile");}
        held.push_back(std::move(body));
    }
    require(cache.statistics().bodies<=512,"held references increased cache capacity");
    RenderPreparationBudget one;one.maxCoverageSteps=1;unsigned calls=0;
    while(!pending->ready()){PreparationAllowance allowance(one);cache.advance(pending,allowance);++calls;
        if(allowance.steps!=1||calls>=2048)std::fprintf(stderr,"held job steps=%zu calls=%u failed=%u bodies=%zu advances=%llu\n",allowance.steps,calls,pending->failed(),cache.statistics().bodies,(unsigned long long)cache.statistics().advances);
        require(allowance.steps==1&&calls<2048,"LRU retirement cancelled an in-flight job");}
    cache.clear();require(budget->used()>0,"held leases lost their charge");held.clear();pending.reset();
    require(budget->used()==0,"held aliases/current job last-release leak");
    std::printf("{\"held_ready_bodies\":513,\"pending_job_survived_retirement\":true,\"last_release\":true}\n");
}
void reclaimableWorkingSet(std::shared_ptr<NativeMetalCompletion> completion){
    auto budget=std::make_shared<resources::ResourceBudgetManager>(resources::BudgetLimits{32U*1024U*1024U,96U*1024U*1024U});auto service=std::make_shared<PreparedGeometryCache>();
    auto pool=std::make_shared<SourceRetentionPool>();service->setRetentionPool(pool);
    RenderCacheOptions options;unsigned n=0;
    while(budget->used()<75U*1024U*1024U){
        auto e=source();e.record.content.update([&](auto& value){std::get<semantic::CompactStrokeContent>(value).stroke.seed+=++n;});
        e.command=CompactStrokeReferenceCommand{scene::ImmutableValue<semantic::CompactStrokeContent>::alias(e.record.content)};
        auto body=service->request(e,options,budget);require(bool(body)&&n<512,"reclaimable cache fill");
        while(!body->ready()){PreparationAllowance allowance;service->advance(body,allowance);require(!body->failed(),"reclaimable body compile");}
    }
    const auto filled=budget->used();const auto evictions=service->statistics().evictions;
    // Full DPR4 original-review target: two staging buffers are48.8MiB.
    auto base=workingSetPlan(513,false,1);
    FrameState frame{ViewId{1},CameraState{{400,250},1,0,CameraGeneration{1}},{0,0,800,500},
        {800,500,3200,2000,4,1},semantic::SemanticGeneration{1},SceneRevision{1},SurfaceGeneration{1},MetricsGeneration{1},FrameId{1},{0,0,800,500}};
    ReferenceDrawList draw{frame};draw.entries=base.referenceDrawList.entries;draw.viewportClip={0,0,800,500};FramePlan plan{frame,std::move(draw),{},false,true};
    NativeMetalTarget target(completion);require(target.resize(3200,2000).code==BackendSubmissionCode::kAccepted,"pressure target");
    auto acquired=target.acquire();acquired.frame.surface->getCanvas()->clear(SK_ColorMAGENTA);
    SkiaRenderCache cache;cache.budget=budget;cache.sourcePool=pool;cache.preparedGeometry=service;
    // Retention pool binding must precede fill; restore service's existing pool
    // is not available outside the owner. This assertion guards the setup.
    require(budget->used()>=filled,"pressure fixture dropped its own prepared cache");
    cache.begin(plan,&target,*acquired.frame.surface,0);unsigned calls=0;
    for(;;){RenderPreparationBudget slice;auto status=cache.prepare(*acquired.frame.surface->getCanvas(),plan,slice,nullptr,nullptr);++calls;
        if(status==SkiaRenderCache::Preparation::kReady)break;
        require(status!=SkiaRenderCache::Preparation::kRejected&&calls<2048,"reclaimable byte pressure failed progress");
        completion->context->flushAndSubmit(GrSyncCpu::kYes);
    }
    require(service->statistics().evictions>evictions,"work buffers did not reclaim optional prepared storage");
    require(cache.compositePreparedFrame(*acquired.frame.surface->getCanvas()),"pressure staging not consumed");target.release();require(target.present().code==BackendSubmissionCode::kAccepted,"pressure present");
    const auto before=budget->used();cache.clear();service->clear();completion->context->flushAndSubmit(GrSyncCpu::kYes);
    require(budget->used()==0,"pressure owner release leaked");
    std::printf("{\"reclaimable_bytes_before\":%zu,\"resident_before_release\":%zu,\"actual_staged_calls\":%u,\"reclaimed\":true,\"last_release\":true}\n",filled,before,calls);
}
void realWorkingSets(){
    auto pool=message(message(reinterpret_cast<id>(objc_getClass("NSAutoreleasePool")),"alloc"),"init");
    {
        const auto device=MTLCreateSystemDefaultDevice(),queue=message(device,"newCommandQueue");
        auto completion=std::make_shared<NativeMetalCompletion>();GrMtlBackendContext backend{sk_cfp<GrMTLHandle>(device),sk_cfp<GrMTLHandle>(queue)};
        completion->context=GrDirectContexts::MakeMetal(backend);require(bool(completion->context),"working-set Metal context");
        heldWorkingSet();reclaimableWorkingSet(completion);
        for(bool equal:{false,true})for(bool one:{false,true}){
            auto plan=workingSetPlan(513,equal,one?1:4);NativeMetalTarget target(completion),reference(completion);
            require(target.resize(plan.frame.metrics.physicalWidth,plan.frame.metrics.physicalHeight).code==BackendSubmissionCode::kAccepted,"working-set target");
            require(reference.resize(plan.frame.metrics.physicalWidth,plan.frame.metrics.physicalHeight).code==BackendSubmissionCode::kAccepted,"working-set reference target");
            auto acquired=target.acquire();acquired.frame.surface->getCanvas()->clear(SK_ColorMAGENTA);target.release();
            std::vector<std::uint8_t> initial(plan.frame.metrics.physicalWidth*plan.frame.metrics.physicalHeight*4),actual(initial.size()),expected(initial.size());
            require(target.readbackRgba(initial).code==BackendSubmissionCode::kAccepted,"working-set initial read");
            RenderPreparationBudget slice;if(one)slice.maxCoverageSteps=1;SkiaRenderer renderer;unsigned calls=0;
            for(;;){const auto before=renderer.renderCacheWork();auto result=renderer.renderFrame(target,plan,nullptr,&slice);++calls;const auto after=renderer.renderCacheWork();
                if(one)require(after.preparedSupportKnots-before.preparedSupportKnots<=32&&after.preparedCompileInputs-before.preparedCompileInputs<=32,"frame aggregate body advance exceeded one quantum");
                require(after.bodySupportKnots==0&&after.bodyLengthInputs==0&&after.bodyCompileInputs==0,"working set used legacy full support/length/compiler leaf");
                if(result.code==BackendSubmissionCode::kAccepted)break;
                require(result.code==BackendSubmissionCode::kRetry&&calls<16384,"valid stable working set did not advance to present");
                require(target.presentCount()==0,"working-set pending presented");
                if(calls==1||calls==2000)require(target.readbackRgba(actual).code==BackendSubmissionCode::kAccepted&&actual==initial,"working-set Pending wrote final target");
            }
            auto work=renderer.renderCacheWork();require(work.preparedBodyBuilds==(equal?1:513)&&work.preparedCanonicalDraws>0,"working-set build/actual canonical consumption");
            require(work.preparedMaxAdvanceKnots<=32,"working-set source quantum");
            SkiaRenderer independent;RenderCacheOptions disabled;disabled.enabled=false;independent.setRenderCacheOptions(disabled);
            require(independent.renderFrame(reference,plan).code==BackendSubmissionCode::kAccepted,"independent working-set GPU reference");
            require(target.readbackRgba(actual).code==BackendSubmissionCode::kAccepted&&reference.readbackRgba(expected).code==BackendSubmissionCode::kAccepted,"working-set canonical pixels");
            // All equal/short sources occupy the first64 samples of the long
            // source. Use the independent CPU edge bands of both geometries.
            auto edge=workingSetEdges(plan.referenceDrawList.entries.front(),one?1:4);
            if(!equal){const auto shortEdge=workingSetEdges(plan.referenceDrawList.entries.back(),one?1:4);for(std::size_t i=0;i<edge.size();++i)edge[i]=edge[i]||shortEdge[i];}
            std::size_t bad=0;for(std::size_t i=0;i<actual.size();++i)if(!edge[i/4]&&std::abs(int(actual[i])-int(expected[i]))>2)++bad;
            if(bad){
                const auto prefix=std::string("out/G5-UR1c2-working-set-repair-r1/working-pixels-")+std::to_string(equal)+"-"+std::to_string(one);
                std::ofstream(prefix+"-actual.rgba",std::ios::binary).write(reinterpret_cast<const char*>(actual.data()),actual.size());
                std::ofstream(prefix+"-reference.rgba",std::ios::binary).write(reinterpret_cast<const char*>(expected.data()),expected.size());
                std::fprintf(stderr,"working set non-AA failures=%zu equal=%u one=%u\n",bad,equal,one);
            }
            require(!bad,"working-set staging changed independent non-AA canonical pixels");
            auto edit=plan.referenceDrawList;for(auto& e:edit.entries){e.record.transform.tx+=.125;
                e.presentationOpacity=.75F;e.presentationEraseActive=true;semantic::SweptCircleMask mask;
                mask.segments.push_back({{{4,35},5},{{4,95},5},{4,50},{4,80}});e.presentationEraseMasks={{{},mask}};}
            FramePlan editing{plan.frame,std::move(edit),{},false,true};EditingOverlay overlay{editing.frame};unsigned editCalls=0;
            for(;;){auto result=renderer.renderTransientOverlay(target,editing,&overlay);++editCalls;if(result.code==BackendSubmissionCode::kAccepted)break;
                require(result.code==BackendSubmissionCode::kRetry&&editCalls<2048,"working-set Direct failed stable demand");
                require(target.presentCount()==1,"Direct pending presented");}
            require(renderer.renderCacheWork().preparedDirectDraws>0,"working-set Direct did not actually consume prepared geometry");
            require(renderer.renderCacheWork().bodyCompileInputs==0&&renderer.renderCacheWork().bodySupportKnots==0,"Direct working-set legacy leaf");
            require(independent.renderTransientOverlay(reference,editing,&overlay).code==BackendSubmissionCode::kAccepted,"independent Direct mask/opacity reference");
            require(target.readbackRgba(actual).code==BackendSubmissionCode::kAccepted&&reference.readbackRgba(expected).code==BackendSubmissionCode::kAccepted,"working-set Direct pixels");
            // Mask geometry and fractional transforms require a new independent
            // band, never a candidate-derived mask or enlarged AA allowance.
            auto maskEntry=editing.referenceDrawList.entries.front();maskEntry.record.eraseMasks=maskEntry.presentationEraseMasks;
            edge=workingSetEdges(maskEntry,one?1:4);
            if(!equal){auto shortEntry=editing.referenceDrawList.entries.back();shortEntry.record.eraseMasks=shortEntry.presentationEraseMasks;const auto shortEdge=workingSetEdges(shortEntry,one?1:4);for(std::size_t i=0;i<edge.size();++i)edge[i]=edge[i]||shortEdge[i];}
            bad=0;for(std::size_t i=0;i<actual.size();++i)if(!edge[i/4]&&std::abs(int(actual[i])-int(expected[i]))>2)++bad;
            require(!bad,"working-set Direct changed independent non-AA mask/opacity pixels");
            if(equal&&!one){
                semantic::ObjectRecord created;created.id=ObjectId::fromUint64(10000);created.kind=semantic::ObjectKind::kShape;created.kind_version=1;
                created.content=semantic::ShapeContent{1,35,40};created.transform.tx=2;created.transform.ty=40;
                created.properties.entries={{0x100U,semantic::FillStyleValue{semantic::SolidFill{{1,0,0,1}}}}};
                EditingOverlay creationOverlay{plan.frame};creationOverlay.setCreationPreview(created,{});
                edge=workingSetEdges(plan.referenceDrawList.entries.front(),4);
                unsigned creationCalls=0;
                for(;;){auto result=renderer.renderTransientOverlay(target,plan,&creationOverlay);++creationCalls;if(result.code==BackendSubmissionCode::kAccepted)break;
                    require(result.code==BackendSubmissionCode::kRetry&&creationCalls<1024,"staged creation replacement failed progress");}
                require(independent.renderTransientOverlay(reference,plan,&creationOverlay).code==BackendSubmissionCode::kAccepted,"independent creation order reference");
                require(target.readbackRgba(actual).code==BackendSubmissionCode::kAccepted&&reference.readbackRgba(expected).code==BackendSubmissionCode::kAccepted,"staged creation pixels");
                bad=0;for(std::size_t i=0;i<actual.size();++i)if(!edge[i/4]&&std::abs(int(actual[i])-int(expected[i]))>2)++bad;
                require(!bad,"staging changed creation-below-ink z-order");
            }
            std::printf("{\"working_set\":513,\"equal_allocations\":%s,\"one_step\":%s,\"canonical_calls\":%u,\"direct_calls\":%u,\"canonical_body_builds\":%llu,\"actual_direct_draws\":%llu,\"legacy_compile\":0,\"pixels\":true}\n",equal?"true":"false",one?"true":"false",calls,editCalls,(unsigned long long)work.preparedBodyBuilds,(unsigned long long)renderer.renderCacheWork().preparedDirectDraws);
        }
    }
    message(pool,"drain");
}
#endif
bool mechanism(){
#if defined(__APPLE__)
    auto pool=message(message(reinterpret_cast<id>(objc_getClass("NSAutoreleasePool")),"alloc"),"init");
    bool reduced=true;
    {
        const auto device=MTLCreateSystemDefaultDevice();require(device!=nil,"Metal device");
        const auto queue=message(device,"newCommandQueue");GrMtlBackendContext backend{sk_cfp<GrMTLHandle>(device),sk_cfp<GrMTLHandle>(queue)};
        auto completion=std::make_shared<NativeMetalCompletion>();completion->context=GrDirectContexts::MakeMetal(backend);require(bool(completion->context),"real Metal context");
        for(unsigned copies:{1U,8U,16U,32U}){
            auto entry=source();semantic::ObjectRecord object;object.id=entry.record.objectId;object.kind=semantic::ObjectKind::kVectorStroke;object.kind_version=3;object.content=entry.record.content.get();
            const auto projection=projectRuntimeScene({&object,1},semantic::SemanticGeneration{1});entry.record=projection.records.front();
            entry.command=CompactStrokeReferenceCommand{scene::ImmutableValue<semantic::CompactStrokeContent>::alias(entry.record.content)};
            auto base=renderPlan(entry,1,1);auto draw=base.referenceDrawList;for(unsigned n=1;n<copies;++n){auto e=entry;e.record.objectId=ObjectId::fromUint64(n+1);e.record.transform.tx=n*.125;draw.entries.push_back(std::move(e));}
            FramePlan plan{base.frame,std::move(draw),{},false,true};
            NativeMetalTarget target(completion);require(target.resize(256,128).code==BackendSubmissionCode::kAccepted,"real target");
            SkiaRenderer renderer;RenderPreparationBudget slice;unsigned attempts=0;
            for(;;){const auto result=renderer.renderFrame(target,plan,nullptr,&slice);++attempts;
                if(result.code==BackendSubmissionCode::kAccepted)break;
                require(result.code==BackendSubmissionCode::kRetry&&attempts<2048,"canonical preparation failed to converge");
                require(target.presentCount()==0,"pending canonical presented");}
            const auto canonical=renderer.renderCacheWork();
            semantic::SweptCircleMask mask;mask.segments.push_back({{{100,20},6},{{100,110},6},{100,40},{100,80}});
            auto editDraw=plan.referenceDrawList;for(auto& e:editDraw.entries)if(std::holds_alternative<CompactStrokeReferenceCommand>(e.command)){
                semantic::ObjectRecord edited;edited.id=e.record.objectId;edited.kind=semantic::ObjectKind::kVectorStroke;edited.kind_version=3;edited.content=e.record.content.get();edited.transform=e.record.transform;edited.erase_masks={{{},mask}};
                e.record=projectRuntimeScene({&edited,1},semantic::SemanticGeneration{2}).records.front();
                e.command=CompactStrokeReferenceCommand{scene::ImmutableValue<semantic::CompactStrokeContent>::alias(e.record.content)};
            }
            FramePlan editing{plan.frame,std::move(editDraw),{},false,true};
            EditingOverlay overlay{editing.frame};unsigned editAttempts=0;
            for(;;){const auto result=renderer.renderTransientOverlay(target,editing,&overlay);++editAttempts;if(result.code==BackendSubmissionCode::kAccepted)break;
                require(result.code==BackendSubmissionCode::kRetry&&editAttempts<2048,"Direct preparation failed to converge");}
            const auto all=renderer.renderCacheWork();
            std::printf("{\"copies\":%u,\"knots\":7199,\"canonical_attempts\":%u,\"canonical_support\":%llu,\"canonical_length_inputs\":%llu,\"canonical_compile_inputs\":%llu,\"edit_support\":%llu,\"edit_length_inputs\":%llu,\"edit_compile_inputs\":%llu,\"gpu_tiles\":%llu}\n",copies,attempts,
                (unsigned long long)canonical.bodySupportKnots,(unsigned long long)canonical.bodyLengthInputs,(unsigned long long)canonical.bodyCompileInputs,
                (unsigned long long)(all.bodySupportKnots-canonical.bodySupportKnots),(unsigned long long)(all.bodyLengthInputs-canonical.bodyLengthInputs),(unsigned long long)(all.bodyCompileInputs-canonical.bodyCompileInputs),(unsigned long long)all.executionGpuTiles);
            require(all.preparedBodyBuilds==1&&all.preparedCanonicalDraws>0&&all.preparedDirectDraws>0,"actual canonical/Direct did not consume one prepared body");
            std::printf("{\"actual_consumers\":true,\"copies\":%u,\"body_builds\":%llu,\"unique_support\":%llu,\"unique_compile\":%llu,\"canonical_draws\":%llu,\"direct_draws\":%llu}\n",copies,(unsigned long long)all.preparedBodyBuilds,(unsigned long long)all.preparedSupportKnots,(unsigned long long)all.preparedCompileInputs,(unsigned long long)all.preparedCanonicalDraws,(unsigned long long)all.preparedDirectDraws);
            reduced=reduced&&canonical.bodySupportKnots<=7199&&canonical.bodyLengthInputs<=7199&&canonical.bodyCompileInputs<=7199&&all.bodyCompileInputs==canonical.bodyCompileInputs;
        }
    }
    message(pool,"drain");if(!reduced)std::fprintf(stderr,"ordinary canonical/Direct repeats full-history support/length/compiler preparation\n");return reduced;
#else
    throw std::runtime_error("native GPU evidence requires Metal on this worker");
#endif
}
}
int main(int argc,char** argv){try{if(argc>1&&std::string_view(argv[1])=="--bounded"){boundedService();boundedGeometryVariants();return 0;}
#if defined(__APPLE__)
if(argc>1&&std::string_view(argv[1])=="--pixels"){realOneStepAndPixels();return 0;}
if(argc>1&&std::string_view(argv[1])=="--working-set"){realWorkingSets();return 0;}
#endif
return mechanism()?0:1;}catch(const std::exception& e){std::fprintf(stderr,"%s\n",e.what());return 1;}}
