#include "../src/skia_prepared_geometry.hpp"
#include "canvas/render/skia_renderer.hpp"
#include "canvas/scene/scene_types.hpp"
#include "canvas/render/skia_scene_renderer.hpp"
#include "include/core/SkSurface.h"
#include "include/core/SkCanvas.h"
#include <tuple>
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
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
ReferenceTraversalEntry source(){
    semantic::CompactStrokeContent c;c.stroke.config.version=2;c.stroke.config.algorithmVersion=4;
    c.stroke.config.signalVersion=3;c.stroke.config.size=18;c.stroke.config.smoothing=.5;c.stroke.seed=42;
    for(unsigned i=0;i<7199;++i)c.stroke.knots.push_back({i*.2,64+25*std::sin(i*.023),std::uint16_t(22000+(i%100)*200)});
    ReferenceTraversalEntry e;e.record.objectId=ObjectId::fromUint64(1);e.record.content=std::move(c);
    e.command=CompactStrokeReferenceCommand{scene::ImmutableValue<semantic::CompactStrokeContent>::alias(e.record.content)};
    e.record.referenceGeometryDigest="deliberately-not-an-authority";return e;
}
template<class T>void printWork(const T& before,const T& after){
    if constexpr(requires{after.fingerprintBytes;})std::printf(
        ",\"fingerprint_bytes\":%llu,\"content_comparisons\":%llu,\"candidate_checks\":%llu,\"identity_hits\":%llu",
        static_cast<unsigned long long>(after.fingerprintBytes-before.fingerprintBytes),
        static_cast<unsigned long long>(after.contentComparisons-before.contentComparisons),
        static_cast<unsigned long long>(after.candidateChecks-before.candidateChecks),
        static_cast<unsigned long long>(after.identityHits-before.identityHits));
    else std::printf(",\"component_counters\":null");
}
template<class T,class S>void printCold(const T& g,const S& s){
    if constexpr(requires{g.fingerprintBytes;s.footprintEvaluations;})std::printf(
        ",\"cold_lookups\":%llu,\"cold_fingerprint_bytes\":%llu,\"cold_content_comparisons\":%llu,\"cold_candidate_checks\":%llu,\"cold_source_footprint_evaluations\":%llu,\"cold_source_admissions\":%llu",
        static_cast<unsigned long long>(g.lookups),static_cast<unsigned long long>(g.fingerprintBytes),
        static_cast<unsigned long long>(g.contentComparisons),static_cast<unsigned long long>(g.candidateChecks),
        static_cast<unsigned long long>(s.footprintEvaluations),static_cast<unsigned long long>(s.admissions));
}
template<class T>void printSourceWork(const T& before,const T& after){
    if constexpr(requires{after.footprintEvaluations;})std::printf(",\"source_footprint_evaluations\":%llu,\"known_footprint_hits\":%llu",
        static_cast<unsigned long long>(after.footprintEvaluations-before.footprintEvaluations),
        static_cast<unsigned long long>(after.knownFootprintHits-before.knownFootprintHits));
}
bool mechanism(){
    bool warmIsIndexed=true;
    for(bool distinct:{false,true})for(unsigned copies:{1U,8U,16U,32U}){
        auto budget=std::make_shared<resources::ResourceBudgetManager>();auto pool=std::make_shared<SourceRetentionPool>();
        PreparedGeometryCache cache;cache.setRetentionPool(pool);RenderCacheOptions options;
        const auto original=source();std::vector<ReferenceTraversalEntry> entries;
        for(unsigned i=0;i<copies;++i){
            auto e=original;e.record.objectId=ObjectId::fromUint64(i+1);e.record.transform.tx=i*.25;
            if(distinct){e.record.content=semantic::ObjectContent{original.record.content.get()};
                e.command=CompactStrokeReferenceCommand{scene::ImmutableValue<semantic::CompactStrokeContent>::alias(e.record.content)};}
            entries.push_back(std::move(e));
        }
        const auto cold=std::chrono::steady_clock::now();
        for(const auto& e:entries)require(bool(cache.get(e,options,budget)),"7199-knot cold prepared admission");
        const auto coldNs=std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now()-cold).count();
        const auto before=cache.statistics();const auto sourceBefore=pool->statistics();const auto warm=std::chrono::steady_clock::now();
        for(unsigned n=0;n<48;++n)for(const auto& e:entries)require(bool(cache.get(e,options,budget)),"warm prepared admission");
        const auto warmNs=std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now()-warm).count();
        const auto after=cache.statistics();const auto sourceAfter=pool->statistics();
        const auto retentionWork=sourceAfter.hits-sourceBefore.hits+sourceAfter.admissions-sourceBefore.admissions;
        warmIsIndexed=warmIsIndexed&&retentionWork==0&&after.builds==before.builds;
        std::printf("{\"distinct_allocations\":%s,\"knots\":7199,\"copies\":%u,\"warm_rounds\":48,\"cold_ns\":%lld,\"warm_ns\":%lld,\"builds\":%llu,\"warm_retention_lookups\":%llu",
            distinct?"true":"false",copies,static_cast<long long>(coldNs),static_cast<long long>(warmNs),
            static_cast<unsigned long long>(after.builds),static_cast<unsigned long long>(retentionWork));
        printCold(before,sourceBefore);printWork(before,after);printSourceWork(sourceBefore,sourceAfter);std::printf("}\n");
        cache.clear();require(budget->used()==0,"mechanism source/index leases leaked");
    }
    return warmIsIndexed;
}
void collisionForksAndSourceProof(){
    PreparedGeometryCache cache;RenderCacheOptions limits;auto budget=std::make_shared<resources::ResourceBudgetManager>();
    auto a=source();auto first=cache.get(a,limits,budget);require(bool(first),"fork source");
    auto b=a;b.record.objectId=ObjectId::fromUint64(2);b.record.content=semantic::ObjectContent{a.record.content.get()};
    b.command=CompactStrokeReferenceCommand{scene::ImmutableValue<semantic::CompactStrokeContent>::alias(b.record.content)};
    const auto bRecord=b.record;auto second=cache.get(b,limits,budget);require(second==first,"equal allocation not completely compared/shared");
    const auto warm=cache.statistics();require(cache.get(b,limits,budget)==first,"equal allocation alias not remembered");
    require(cache.statistics().contentComparisons==warm.contentComparisons&&cache.statistics().fingerprintBytes==warm.fingerprintBytes,"warm alias rehash/recompare");
    for(unsigned field=0;field<3;++field){
        auto changed=a;changed.record.content.update([&](auto& value){auto& c=std::get<semantic::CompactStrokeContent>(value);
            if(field==0)c.stroke.config.size+=1;if(field==1)++c.stroke.seed;if(field==2)c.stroke.knots[20].y+=1;});
        changed.command=CompactStrokeReferenceCommand{scene::ImmutableValue<semantic::CompactStrokeContent>::alias(changed.record.content)};
        require(cache.get(changed,limits,budget)!=first,"same ID/digest/length accepted a changed body");
    }
    require(cache.statistics().contentComparisons>=3,"config/seed fingerprint collisions did not take full equality");
    auto transformed=a;transformed.record.transform.tx=99;
    semantic::SweptCircleMask m;m.segments.push_back({{{45,20},8},{{45,100},8},{45,40},{45,80}});
    transformed.record.eraseMasks={{{},m}};transformed.presentationEraseActive=true;
    require(cache.get(transformed,limits,budget)==first&&b.record==bRecord,"A transform/mask fork changed B/body");
    require(cache.get(a,limits,budget)==first&&cache.get(transformed,limits,budget)==first,"Undo/Redo body identity lost");
    auto pool=std::make_shared<SourceRetentionPool>();ReferenceTraversalEntry aliased;
    semantic::BrushStrokeContent vector;vector.stroke.snapshot.material_mode=1;vector.stroke.vector_output.outline={{0,0},{10,0},{0,10}};
    scene::ImmutableValue<std::tuple<semantic::CompactStrokeContent,semantic::BrushStrokeContent>> owner{
        std::tuple{std::get<semantic::CompactStrokeContent>(a.record.content.get()),vector}};
    aliased.command=CompactStrokeReferenceCommand{scene::ImmutableValue<semantic::CompactStrokeContent>::alias(owner)};
    auto held=pool->get(aliased,budget,true);require(bool(held),"same owner first pointee source");const auto before=pool->statistics();
    aliased.command=BrushStrokeReferenceCommand{scene::ImmutableValue<semantic::BrushStrokeContent>::alias(owner)};
    auto other=pool->get(aliased,budget,true);require(bool(other)&&pool->statistics().footprintEvaluations==before.footprintEvaluations+1,"unknown aliased pointee/type skipped footprint validation");
    const auto validated=pool->statistics().footprintEvaluations;
    require(bool(pool->get(aliased,budget,true))&&pool->statistics().footprintEvaluations==validated,"known aliased view recomputed footprint");
    PreparedGeometryCache isolated;auto compactAlias=aliased;
    compactAlias.command=CompactStrokeReferenceCommand{scene::ImmutableValue<semantic::CompactStrokeContent>::alias(owner)};
    auto one=isolated.get(compactAlias,limits,budget),two=isolated.get(aliased,limits,budget);
    require(one&&two&&one!=two&&one->isContour()&&two->vectorPath(),"same owner different typed pointees conflated");
    isolated.clear();one.reset();two.reset();held.reset();other.reset();cache.clear();first.reset();second.reset();
    require(budget->used()==0,"collision/source alias cleanup");
}
void identityAndUnknownFootprint(){
    auto a=std::make_shared<const std::array<int,2>>(std::array<int,2>{1,2});auto b=std::make_shared<const int>(7);
    ImmutableSourceIdentity first{std::shared_ptr<const void>(a,&(*a)[0]),sourceTypeTag<int>(),1};
    ImmutableSourceIdentity otherPointee{std::shared_ptr<const void>(a,&(*a)[1]),sourceTypeTag<int>(),1};
    ImmutableSourceIdentity otherOwner{std::shared_ptr<const void>(b,&(*a)[0]),sourceTypeTag<int>(),1};
    auto otherType=first;otherType.type=sourceTypeTag<double>();
    ImmutableSourceIdentityLess less;
    for(const auto& key:{otherPointee,otherOwner,otherType})require(less(first,key)||less(key,first),"owner/pointee/type identity collapsed or ABA");
    auto content=std::get<semantic::CompactStrokeContent>(source().record.content.get());content.stroke.knots.resize(2);
    semantic::BrushStrokeContent large;large.stroke.vector_output.outline.resize(20000);large.stroke.snapshot.material_mode=1;
    scene::ImmutableValue<std::tuple<semantic::CompactStrokeContent,semantic::BrushStrokeContent>> owner{std::tuple{content,large}};
    auto budget=std::make_shared<resources::ResourceBudgetManager>();SourceRetentionPool pool;ReferenceTraversalEntry entry;
    entry.command=CompactStrokeReferenceCommand{scene::ImmutableValue<semantic::CompactStrokeContent>::alias(owner)};
    auto firstSource=pool.get(entry,budget,true);require(bool(firstSource),"unknown footprint fixture");const auto before=pool.statistics();const auto charged=budget->used();
    entry.command=BrushStrokeReferenceCommand{scene::ImmutableValue<semantic::BrushStrokeContent>::alias(owner)};
    require(!pool.get(entry,budget,true)&&pool.statistics().footprintEvaluations==before.footprintEvaluations+1&&budget->used()==charged,"larger unproven aliased footprint reused small charge");
    firstSource.reset();require(budget->used()==0&&pool.statistics().indexBytes==0,"source index survived last retention");
}
void sourceRetentionScale(){
    auto budget=std::make_shared<resources::ResourceBudgetManager>();SourceRetentionPool pool;
    std::vector<SourceRetentions> holds;ReferenceTraversalEntry last;
    for(unsigned n=0;n<1100;++n){
        ReferenceTraversalEntry e;e.record.objectId=ObjectId::fromUint64(n+1);
        e.record.content=semantic::ShapeContent{1,16,16};e.command=ShapeReferenceCommand{semantic::ShapeContent{1,16,16}};
        auto retained=pool.get(e,budget,true);
        if(!retained)std::fprintf(stderr,"source index denied at object=%u admissions=%llu charged=%zu\n",n,static_cast<unsigned long long>(pool.statistics().admissions),budget->used());
        require(bool(retained),"source index introduced an artificial1024owner cap below existing budget");holds.push_back(std::move(*retained));last=std::move(e);
    }
    const auto before=pool.statistics().footprintEvaluations;require(bool(pool.get(last,budget,true))&&pool.statistics().footprintEvaluations==before,"large live source index lost known footprint");
    std::printf("{\"source_index_objects\":1100,\"actual_retained_allocations\":%llu,\"accounted_bytes\":%zu,\"index_reserved_bytes\":%zu}\n",static_cast<unsigned long long>(pool.statistics().admissions),budget->used(),pool.statistics().indexBytes);
    holds.clear();require(budget->used()==0&&pool.statistics().indexBytes==0,"large source index last-release cleanup");
}
void aliasPressureRecovery(){
    constexpr std::size_t capacity=4*1024*1024;
    auto budget=std::make_shared<resources::ResourceBudgetManager>(resources::BudgetLimits{capacity,capacity});
    PreparedGeometryCache cache;RenderCacheOptions limits;auto original=source();auto held=cache.get(original,limits,budget);require(bool(held),"alias pressure body");
    const auto copy=[&](unsigned id){auto e=original;e.record.objectId=ObjectId::fromUint64(id);e.record.content=semantic::ObjectContent{original.record.content.get()};
        e.command=CompactStrokeReferenceCommand{scene::ImmutableValue<semantic::CompactStrokeContent>::alias(e.record.content)};return e;};
    for(unsigned n=0;n<8;++n)require(cache.get(copy(n+2),limits,budget)==held,"alias pressure admission");
    const auto before=cache.statistics();const auto token=budget->reserve(resources::ResourceClass::kScratch,capacity-budget->used()-65536,true);require(bool(token),"alias pressure reservation");
    resources::ResourceLease pressure(budget,*token);
    require(cache.get(copy(99),limits,budget)==held,"new identity cannot reclaim cache-only aliases while body is pinned");
    require(cache.statistics().aliasEvictions>before.aliasEvictions&&cache.statistics().builds==1&&budget->used()<=capacity,"alias quota recovery changed body or cap");
    pressure.reset();cache.clear();held.reset();require(budget->used()==0,"alias pressure source/index lease leak");
}
void quotaAliasAndHeldLifetimes(){
    auto budget=std::make_shared<resources::ResourceBudgetManager>();PreparedGeometryCache cache;RenderCacheOptions limits;limits.maxEntries=4;
    const auto original=source();auto held=cache.get(original,limits,budget);require(bool(held),"LRU pinned body");
    for(unsigned i=0;i<32;++i){auto e=original;e.record.objectId=ObjectId::fromUint64(i+2);
        e.record.content=semantic::ObjectContent{original.record.content.get()};
        e.command=CompactStrokeReferenceCommand{scene::ImmutableValue<semantic::CompactStrokeContent>::alias(e.record.content)};
        require(cache.get(e,limits,budget)==held&&cache.statistics().aliases<=limits.maxEntries,"alias capacity/complete sharing");}
    require(cache.statistics().aliasEvictions>0&&cache.statistics().builds==1,"bounded aliases pinned extra bodies");
    for(unsigned i=0;i<12;++i){auto e=original;e.record.content.update([&](auto& value){std::get<semantic::CompactStrokeContent>(value).stroke.seed+=i+1;});
        e.command=CompactStrokeReferenceCommand{scene::ImmutableValue<semantic::CompactStrokeContent>::alias(e.record.content)};
        require(bool(cache.get(e,limits,budget)),"unheld body could not be evicted while another consumer pins body");}
    require(cache.statistics().evictions>0&&cache.get(original,limits,budget)==held,"LRU invalidated held body");
    require(cache.statistics().residentBytes==budget->used(),"source/body/index reservation not accounted exactly once");
    cache.clear();require(budget->used()>0,"clear invalidated held geometry/source/index reservation");held.reset();require(budget->used()==0,"last geometry/index/source release leaked");
    auto denied=std::make_shared<resources::ResourceBudgetManager>(resources::BudgetLimits{128,128});
    require(!cache.get(original,limits,denied)&&denied->used()==0,"quota denial retained partial index/source");
    auto recovered=cache.get(original,limits,budget);require(bool(recovered),"same demand did not recover after quota owner change");
    auto tiny=std::make_shared<resources::ResourceBudgetManager>(resources::BudgetLimits{1,1});
    require(!cache.get(original,limits,tiny)&&budget->used()>0&&tiny->used()==0,"old live charge moved to new budget");
    recovered.reset();cache.clear();require(budget->used()==0,"retired old budget source/index leaked");
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
void realRendererConsumers(){
    auto pool=message(message(reinterpret_cast<id>(objc_getClass("NSAutoreleasePool")),"alloc"),"init");
    {
        const auto device=MTLCreateSystemDefaultDevice();require(device!=nil,"native Metal device missing");
        const auto queue=message(device,"newCommandQueue");require(queue!=nil,"native Metal command queue missing");
        GrMtlBackendContext backend{sk_cfp<GrMTLHandle>(device),sk_cfp<GrMTLHandle>(queue)};
        auto completion=std::make_shared<NativeMetalCompletion>();completion->context=GrDirectContexts::MakeMetal(backend);require(bool(completion->context),"locked Skia SDK Metal context");
        for(unsigned dpr:{1U,2U,4U}){
            auto entry=source();semantic::ObjectRecord object;object.id=entry.record.objectId;object.kind=semantic::ObjectKind::kVectorStroke;object.kind_version=3;object.content=entry.record.content.get();object.transform={.98,.1,-.1,.98,7,-6};object.placement.order_key=semantic::OrderKey({2});
            std::get<semantic::CompactStrokeContent>(object.content).stroke.config.opacity=.55;
            const auto projection=projectRuntimeScene({&object,1},semantic::SemanticGeneration{1});entry.record=projection.records.front();
            entry.command=CompactStrokeReferenceCommand{scene::ImmutableValue<semantic::CompactStrokeContent>::alias(entry.record.content)};
            NativeMetalTarget target(completion);require(target.resize(256*dpr,128*dpr).code==BackendSubmissionCode::kAccepted,"Metal target resize");
            SkiaRenderer renderer;RenderCacheOptions options;options.navigationMaxSteps=256;options.navigationMicroseconds=1000000;renderer.setRenderCacheOptions(options);
            auto plan=renderPlan(entry,1,dpr);NavigationSourceQuery query=[&](const auto&,auto& entries){entries=plan.referenceDrawList.entries;return true;};
            for(unsigned n=0;n<256;++n){require(renderer.renderNavigation(target,{1,0,0},plan.frame,query).code==BackendSubmissionCode::kAccepted,"actual renderer navigation");if(!renderer.navigationWork().pending)break;require(n!=255,"navigation failed to converge");}
            const auto nav=renderer.renderCacheWork();require(nav.preparedBodyBuilds==1&&nav.preparedNavigationQueries>0,"navigation did not prepare one shared body");
            renderer.cancelNavigationWork();auto edit=entry;edit.presentationEraseActive=true;
            semantic::SweptCircleMask mask;mask.segments.push_back({{{65,30},8},{{65,95},8},{65,45},{65,80}});edit.presentationEraseMasks={{{},mask}};
            auto editing=renderPlan(edit,2,dpr);EditingOverlay overlay{editing.frame};
            require(renderer.renderTransientOverlay(target,editing,&overlay).code==BackendSubmissionCode::kAccepted,"actual full transient retained ink");
            NativeMetalTarget reference(completion);require(reference.resize(256*dpr,128*dpr).code==BackendSubmissionCode::kAccepted,"Metal pixel reference target");
            SkiaRenderer independent;RenderCacheOptions uncached;uncached.enabled=false;independent.setRenderCacheOptions(uncached);
            require(independent.renderFrame(reference,editing).code==BackendSubmissionCode::kAccepted,"independent GPU Direct pixel reference");
            std::vector<std::uint8_t> actual(256*128*dpr*dpr*4),expected(actual.size());
            require(target.readbackRgba(actual).code==BackendSubmissionCode::kAccepted&&reference.readbackRgba(expected).code==BackendSubmissionCode::kAccepted,"native GPU pixel readbacks");
            require(actual==expected,"shared prepared native GPU changed transform/mask/opacity/lower-z pixels");
            const auto ink=renderer.renderCacheWork();
            require(ink.preparedInkQueries>0&&ink.preparedBodyBuilds==nav.preparedBodyBuilds&&ink.preparedIdentityHits>nav.preparedIdentityHits,"actual consumers rebuilt/separated prepared service");
            require(ink.preparedHashBytes==nav.preparedHashBytes&&ink.preparedContentComparisons==nav.preparedContentComparisons,"retained ink rehashed/recompared admitted body");
            std::printf("{\"actual_native_gpu\":true,\"dpr\":%u,\"shared_body_builds\":%llu,\"navigation_queries\":%llu,\"retained_ink_queries\":%llu,\"identity_hits\":%llu}\n",dpr,
                static_cast<unsigned long long>(ink.preparedBodyBuilds),static_cast<unsigned long long>(ink.preparedNavigationQueries),static_cast<unsigned long long>(ink.preparedInkQueries),static_cast<unsigned long long>(ink.preparedIdentityHits));
            require(target.advanceGeneration().code==BackendSubmissionCode::kAccepted,"native target rebind");
            require(renderer.renderNavigation(target,{1,0,0},plan.frame,query).code!=BackendSubmissionCode::kAccepted,"stale navigation presented");
            auto rebound=renderPlan(edit,3,dpr,target.generation());EditingOverlay reboundOverlay{rebound.frame};
            require(renderer.renderTransientOverlay(target,rebound,&reboundOverlay).code==BackendSubmissionCode::kAccepted,"full transient after consumer surface reset");
            require(renderer.renderCacheWork().preparedBodyBuilds==ink.preparedBodyBuilds&&renderer.renderCacheWork().preparedInkQueries>ink.preparedInkQueries,"consumer reset cleared another consumer's shared body");
            renderer.clearRenderCache();renderer.serviceResourceRetirements();
            require(renderer.renderCacheWork().preparedAliases==0&&renderer.renderCacheWork().preparedIndexReservationBytes==0,"renderer owner clear left source/index metadata");
        }
    }
    message(pool,"drain");
}
#else
void realRendererConsumers(){std::printf("{\"native_gpu_consumer_sequence\":\"UNAVAILABLE; real Web qualification required\"}\n");}
#endif

}
int main(int argc,char** argv){try{
    if(argc>1&&std::string_view(argv[1])=="--source-scale-only"){sourceRetentionScale();return 0;}
    if(argc>1&&std::string_view(argv[1])=="--alias-pressure-only"){aliasPressureRecovery();return 0;}
    const bool indexed=mechanism();if(argc>1&&std::string_view(argv[1])=="--measure-only")return 0;
    require(indexed,"warm proven prepared identity re-enters source retention/content-proportional work");
    collisionForksAndSourceProof();identityAndUnknownFootprint();sourceRetentionScale();aliasPressureRecovery();quotaAliasAndHeldLifetimes();realRendererConsumers();return 0;
}catch(const std::exception& e){std::fprintf(stderr,"%s\n",e.what());return 1;}}
