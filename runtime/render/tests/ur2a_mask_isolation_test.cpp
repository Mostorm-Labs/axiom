#include "../src/skia_geometry_internal.hpp"
#include "canvas/render/skia_renderer.hpp"
#include "canvas/render/skia_scene_renderer.hpp"
#include "canvas/ink/brush_package_catalog.hpp"
#include "canvas/scene/scene_types.hpp"
#include "include/core/SkCanvas.h"
#include "include/core/SkSurface.h"
#include "include/gpu/ganesh/mtl/GrMtlBackendContext.h"
#include "include/gpu/ganesh/mtl/GrMtlDirectContext.h"
#include "include/gpu/ganesh/GrDirectContext.h"
#include "include/gpu/ganesh/SkSurfaceGanesh.h"
#include <objc/message.h>
#include <objc/runtime.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <stdexcept>
#include <string_view>
extern "C" id MTLCreateSystemDefaultDevice(void);
using namespace canvas;
using namespace canvas::render;
namespace {
void require(bool value,const char* label){if(!value)throw std::runtime_error(label);}
// Existing native Ganesh/Metal fixture realization, matching gpu_resource_recovery_test.
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
std::shared_ptr<NativeMetalCompletion> context(){
    const auto device=MTLCreateSystemDefaultDevice(),queue=message(device,"newCommandQueue");
    GrMtlBackendContext backend{sk_cfp<GrMTLHandle>(device),sk_cfp<GrMTLHandle>(queue)};
    auto c=std::make_shared<NativeMetalCompletion>();c->context=GrDirectContexts::MakeMetal(backend);
    require(bool(c->context),"actual_Metal_context");return c;
}
ReferenceTraversalEntry maskedStroke(semantic::Transform2D transform,unsigned algorithm=4){
 semantic::ObjectRecord r;r.id=ObjectId::fromUint64(1);r.kind=semantic::ObjectKind::kVectorStroke;r.kind_version=3;
 semantic::CompactStrokeContent content;auto brush=ink::BrushPackageCatalog{}.loadDefault("round-handdrawn",4);require(bool(brush),"package");
 content.stroke.config=*brush.package.compactConfig;content.stroke.config.algorithmVersion=algorithm;if(algorithm==3)content.stroke.config.signalVersion=2;content.stroke.config.size=20;content.stroke.seed=42;
 for(unsigned i=0;i<40;++i)content.stroke.knots.push_back({20.+i*5.,64.,32768});
 r.content=std::move(content);r.transform=transform;r.placement.order_key=semantic::OrderKey({1});
 semantic::VectorPathGeometry mask;mask.commands={semantic::MoveTo{{100,0}},semantic::LineTo{{120,0}},semantic::LineTo{{120,128}},semantic::LineTo{{100,128}},semantic::ClosePath{}};
 r.erase_masks={{ObjectId::fromUint64(51),semantic::FilledPathMask{std::move(mask)}}};
 auto projected=projectRuntimeScene({&r,1},semantic::SemanticGeneration{1});require(projected.records.size()==1,"projected");
 auto record=projected.records.front();return {record,true,CompactStrokeReferenceCommand{scene::ImmutableValue<semantic::CompactStrokeContent>::alias(record.content)}};
}
FramePlan planFor(const ReferenceTraversalEntry& entry,unsigned dpr,std::uint64_t generation,bool isolate=false,bool transparent=false,std::uint64_t revision=1){
 FrameState f{ViewId{1},CameraState{{128,64},1,0,CameraGeneration{1}},{0,0,256,128},{256,128,256*dpr,128*dpr,float(dpr),1},semantic::SemanticGeneration{revision},SceneRevision{revision},SurfaceGeneration{generation},MetricsGeneration{1},FrameId{revision},{0,0,256,128}};
 ReferenceDrawList draw{.frame=f,.entries={entry},.canonicalBytes={},.digest={}};draw.viewportClip={0,0,256,128};
 return {f,std::move(draw),{},transparent,isolate};
}
std::vector<std::uint8_t> pixels(SkiaSurfaceProvider& provider,unsigned dpr){
 std::vector<std::uint8_t> out(std::size_t(256*dpr)*128*dpr*4);require(provider.readbackRgba(out).code==BackendSubmissionCode::kAccepted,"readback");return out;
}
unsigned alphaAt(const std::vector<std::uint8_t>& bytes,unsigned dpr,double x,double y){return bytes[(std::size_t(y*dpr)*256*dpr+std::size_t(x*dpr))*4+3];}

std::vector<std::uint8_t> preparedPixels(SkiaRenderer& renderer,NativeMetalTarget& target,const FramePlan& plan,unsigned dpr){
 RenderPreparationBudget one;one.maxCoverageSteps=1;auto result=BackendSubmissionResult::retry("begin");
 for(unsigned i=0;i<8192&&result.code==BackendSubmissionCode::kRetry;++i)result=renderer.renderFrame(target,plan,nullptr,&one);
 require(result.code==BackendSubmissionCode::kAccepted,"cooperative completion");return pixels(target,dpr);
}
unsigned backdropCases(const std::shared_ptr<NativeMetalCompletion>& gpu){
 for(unsigned dpr:{1U,2U,4U})for(unsigned variant=0;variant<4;++variant){
  auto first=maskedStroke({1,.08,-.06,1,5,0});
  first.record.content.update([](auto& v){std::get<semantic::CompactStrokeContent>(v).stroke.config.opacity=.5;});
  first.command=CompactStrokeReferenceCommand{scene::ImmutableValue<semantic::CompactStrokeContent>::alias(first.record.content)};
  auto second=first;second.record.objectId=ObjectId::fromUint64(2);second.record.transform={.9,0,0,1.1,8,-3};
  auto masks=first.record.eraseMasks;auto extra=masks.front();extra.id=ObjectId::fromUint64(52);masks.push_back(extra);
  NativeMetalTarget target(gpu);require(target.resize(256*dpr,128*dpr).code==BackendSubmissionCode::kAccepted,"backdrop target");
  SkiaRenderer renderer;RenderCacheOptions options;if(variant==3)options.maxEntries=1;renderer.setRenderCacheOptions(options);
  std::vector<std::uint8_t> after;
  for(unsigned state=0;state<5;++state){
   auto entry=first;if(state==1||state>=3)entry.record.eraseMasks=masks;
   const auto base=planFor(entry,dpr,target.generation(),false,variant==2,state+1);auto draw=base.referenceDrawList;
   foundation::DocumentBackground background;if(variant==1)background.color={.25,.5,.75,1};
   if(variant==3){
    semantic::ObjectRecord lower;lower.id=ObjectId::fromUint64(90);lower.kind=semantic::ObjectKind::kShape;lower.kind_version=1;lower.content=semantic::ShapeContent{1,256,128};lower.properties.entries={{0x100U,semantic::FillStyleValue{semantic::SolidFill{{0,0,1,1}}}}};lower.placement.order_key=semantic::OrderKey({0});
    auto projection=projectRuntimeScene({&lower,1},semantic::SemanticGeneration{state+1});
    draw.entries.insert(draw.entries.begin(),ReferenceTraversalEntry{projection.records.front(),true,ShapeReferenceCommand{std::get<semantic::ShapeContent>(lower.content)}});
   }
   draw.entries.push_back(second);const FramePlan actualPlan{base.frame,draw,background,variant==2,false};
   const auto actual=preparedPixels(renderer,target,actualPlan,dpr);
   const auto hole=std::size_t(20*dpr)*256*dpr*4+110*dpr*4;
   if(variant!=2)require(actual[hole+3]==255,"own masks preserve backdrop alpha");
   else require(actual[hole+3]==0,"transparent target remains transparent");
   // Independent direct drawing into a separate real GPU surface; the explicit
   // isolation contract overrides the classifier being tested.
   NativeMetalTarget reference(gpu);require(reference.resize(256*dpr,128*dpr).code==BackendSubmissionCode::kAccepted,"reference target");
   const FramePlan expected{actualPlan.frame,draw,background,variant==2,true};auto a=reference.acquire();require(internal::drawReferencePlanToSkCanvas(*a.frame.surface->getCanvas(),expected).code==BackendSubmissionCode::kAccepted,"isolated reference");reference.release();require(reference.present().code==BackendSubmissionCode::kAccepted,"reference present");
   const auto independent=pixels(reference,dpr);require(actual[hole]==independent[hole]&&actual[hole+1]==independent[hole+1]&&actual[hole+2]==independent[hole+2]&&actual[hole+3]==independent[hole+3],"backdrop color and order");
   const auto overlap=(std::size_t(67*dpr)*256*dpr+80*dpr)*4;
   for(unsigned channel=0;channel<4;++channel)require(std::abs(int(actual[overlap+channel])-int(independent[overlap+channel]))<=2,"translucent overlap keeps source opacity and order");
   // Same camera/material mask states are bitwise repeatable. Full CPU/AA pixel
   // qualification is separately frozen in the actual Web80-stage matrix.
   if(state==1)after=actual;if(state>=3)require(actual==after,"mask-only Redo/cache hit repeatability");
  }
  const auto work=renderer.renderCacheWork();require(work.executionGpuTiles>0,"actual GPU material coverage");
  std::printf("{\"stage\":\"backdrop-order-pressure\",\"dpr\":%u,\"variant\":%u,\"actual_gpu\":true,\"gpu_tiles\":%llu,\"default_shared_budget\":true,\"per_entry_pressure_test_only\":%s,\"pass\":true}\n",dpr,variant,(unsigned long long)work.executionGpuTiles,variant==3?"true":"false");
 }
 return 0;
}
unsigned legacyCases(const std::shared_ptr<NativeMetalCompletion>& gpu){
 for(unsigned kind=0;kind<3;++kind)for(unsigned dpr:{1U,2U,4U}){
  auto entry=maskedStroke({1,0,0,1,5,5},3);
  if(kind<2){
   semantic::StrokeRecord stroke;stroke.brush.brush_family_id=1;stroke.brush.brush_version=1;stroke.brush.nominal_size=20;stroke.brush.opacity=1;stroke.brush.color={0,0,0,1};
   if(kind==0){stroke.data=semantic::VectorStrokeData{{{{20,64},.5,{}},{{200,64},.5,{}}}};entry.record.content=semantic::VectorStrokeContent{std::move(stroke)};entry.command=VectorStrokeReferenceCommand{scene::ImmutableValue<semantic::VectorStrokeContent>::alias(entry.record.content)};}
   else{stroke.data=semantic::DabStrokeData{{{{20,64},20,0,1}}};entry.record.kind=semantic::ObjectKind::kDabStroke;entry.record.content=semantic::DabStrokeContent{std::move(stroke)};entry.command=DabStrokeReferenceCommand{scene::ImmutableValue<semantic::DabStrokeContent>::alias(entry.record.content)};}
   entry.record.kindVersion=1;
  }
  require(!internal::requiresInkEntryIsolation(entry,false),"legacy transformed destination semantics unchanged");
  NativeMetalTarget target(gpu);require(target.resize(256*dpr,128*dpr).code==BackendSubmissionCode::kAccepted,"legacy target");auto plan=planFor(entry,dpr,target.generation());auto a=target.acquire();require(internal::drawReferencePlanToSkCanvas(*a.frame.surface->getCanvas(),plan).code==BackendSubmissionCode::kAccepted,"legacy Direct");target.release();require(target.present().code==BackendSubmissionCode::kAccepted,"legacy present");require(alphaAt(pixels(target,dpr),dpr,115,25)==0,"legacy standalone destination-clear preserved");
  std::printf("{\"stage\":\"legacy-unchanged\",\"kind\":%u,\"dpr\":%u,\"pass\":true}\n",kind,dpr);
 }
 return 0;
}
}
int main(int argc,char** argv){try{
 const std::string_view mode=argc>1?argv[1]:"direct";unsigned failures=0;
 auto gpu=context();
 if(mode=="backdrop")return backdropCases(gpu);if(mode=="legacy")return legacyCases(gpu);
 const std::array<semantic::Transform2D,4> transforms{{{},{1,0,0,1,5,5},{.98,.1,-.1,.98,10,0},{.8,0,0,.9,5,5}}};
 for(unsigned dpr:{1U,2U,4U})for(const auto& transform:transforms){
  const bool translated=transform!=semantic::Transform2D{};
  const auto entry=maskedStroke(transform);NativeMetalTarget target(gpu);require(target.resize(256*dpr,128*dpr).code==BackendSubmissionCode::kAccepted,"target");
  const auto plan=planFor(entry,dpr,target.generation());
  if(mode=="direct"){
   auto acquired=target.acquire();require(acquired.frame.surface&&acquired.frame.surface->recordingContext(),"actual_GPU");
   require(internal::drawReferencePlanToSkCanvas(*acquired.frame.surface->getCanvas(),plan).code==BackendSubmissionCode::kAccepted,"Direct");target.release();require(target.present().code==BackendSubmissionCode::kAccepted,"present");
  }else{
   SkiaRenderer renderer;RenderPreparationBudget one;one.maxCoverageSteps=1;auto result=BackendSubmissionResult::retry("begin");
   for(unsigned i=0;i<8192&&result.code==BackendSubmissionCode::kRetry;++i)result=renderer.renderFrame(target,plan,nullptr,&one);
   require(result.code==BackendSubmissionCode::kAccepted,"prepared completion");
   const auto work=renderer.renderCacheWork();std::printf("{\"stage\":\"actual-preparation\",\"gpu_tiles\":%llu,\"cpu_tiles\":%llu,\"scratch_allocations\":%llu,\"body_builds\":%llu}\n",(unsigned long long)work.executionGpuTiles,(unsigned long long)work.executionCpuTiles,(unsigned long long)work.scratchAllocations,(unsigned long long)work.preparedBodyBuilds);
  }
  const auto actual=pixels(target,dpr);const double x=transform.a*110+transform.c*20+transform.tx,y=transform.b*110+transform.d*20+transform.ty;const auto alpha=alphaAt(actual,dpr,x,y);
  const bool pass=alpha==255;failures+=!pass;
  std::printf("{\"stage\":\"causal-mask-destination\",\"mode\":\"%s\",\"dpr\":%u,\"translated\":%s,\"actual_gpu\":true,\"classifier_isolates\":%s,\"mask_extends_beyond_body\":true,\"mask_probe_alpha\":%u,\"independent_expected_alpha\":255,\"pass\":%s}\n",mode.data(),dpr,translated?"true":"false",internal::requiresInkEntryIsolation(entry,false)?"true":"false",alpha,pass?"true":"false");
  // Expected value is explicit background opacity, not the classifier under test.
  const auto expected=planFor(entry,dpr,target.generation(),true);auto a=target.acquire();require(internal::drawReferencePlanToSkCanvas(*a.frame.surface->getCanvas(),expected).code==BackendSubmissionCode::kAccepted,"independent forced isolation");target.release();require(target.present().code==BackendSubmissionCode::kAccepted,"reference present");
  require(alphaAt(pixels(target,dpr),dpr,x,y)==255,"independent isolated reference opaque");
 }
 return failures?1:0;
}catch(const std::exception& e){std::fprintf(stderr,"SETUP_OR_PROGRESS: %s\n",e.what());return 2;}}
