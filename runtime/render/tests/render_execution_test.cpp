#include "../src/compact_raster_internal.hpp"
#include "canvas/render/skia_renderer.hpp"
#include "canvas/scene/scene_types.hpp"
#include "canvas/ink/brush_package_catalog.hpp"
#include "include/core/SkCanvas.h"
#include <cstdio>
#include <stdexcept>
#include <string_view>

using namespace canvas;
using namespace canvas::render;
namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
void actualTargetKey(){
    auto root=SkSurfaces::Raster(SkImageInfo::MakeN32Premul(32,32));
    require(bool(root),"real raster root");
    internal::CompactCoverageTargets targets;
    targets.budget=std::make_shared<resources::ResourceBudgetManager>();
    internal::CompactRasterWork work;
    auto requestedCompatible=targets.acquire(*root->getCanvas(),16,16,true,&work);
    require(bool(requestedCompatible)&&!requestedCompatible->recordingContext(),"real requested-compatible Raster allocation");
    auto explicitCpu=targets.acquire(*root->getCanvas(),16,16,false,&work);
    std::printf("requested_compatible=1 actual_gpu=0 cpu_reused_same_target=%d allocations=%llu reuses=%llu\n",
        explicitCpu.get()==requestedCompatible.get(),static_cast<unsigned long long>(work.allocatedTiles),static_cast<unsigned long long>(work.targetReuses));
    require(explicitCpu.get()==requestedCompatible.get(),"target retention is keyed by requested-compatible instead of actual Raster kind/context");
}
FramePlan framePlan(std::uint64_t id,unsigned dpr=1,unsigned variant=0){
    semantic::ObjectRecord object;object.id=ObjectId::fromUint64(1);object.kind=semantic::ObjectKind::kVectorStroke;object.kind_version=3;
    auto package=ink::BrushPackageCatalog{}.loadDefault("round-handdrawn",4);require(bool(package),"v4 fixture package");
    auto config=*package.package.compactConfig;config.size=18;config.opacity=.55;
    object.content=semantic::CompactStrokeContent{{config,7,{{20,64,32768},{236,64,48000}}}};
    if(variant){object.transform={.9,.1,-.1,.9,10,-4};object.placement.order_key=semantic::OrderKey({2});}
    if(variant==2){semantic::SweptCircleMask mask;mask.segments.push_back({{{100,54},7},{{100,74},7},{100,60},{100,68}});object.erase_masks={{ObjectId::fromUint64(91),mask}};}
    auto projection=projectRuntimeScene({&object,1},semantic::SemanticGeneration{1});
    ReferenceTraversalEntry entry{projection.records.front(),true,CompactStrokeReferenceCommand{std::get<semantic::CompactStrokeContent>(object.content)}};
    FrameState frame{ViewId{1},CameraState{{128,64},1,0,CameraGeneration{1}},{0,0,256,128},
        {256,128,256*dpr,128*dpr,float(dpr),1},semantic::SemanticGeneration{1},SceneRevision{1},SurfaceGeneration{1},MetricsGeneration{1},FrameId{id},{0,0,256,128}};
    ReferenceDrawList list{frame};list.viewportClip={0,0,256,128};list.entries={entry};
    if(variant){semantic::ObjectRecord backdrop;backdrop.id=ObjectId::fromUint64(2);backdrop.kind=semantic::ObjectKind::kShape;backdrop.kind_version=1;
        backdrop.content=semantic::ShapeContent{1,256,128};backdrop.placement.order_key=semantic::OrderKey({1});
        backdrop.properties.entries.push_back({0x100U,semantic::FillStyleValue{semantic::SolidFill{{0,0,1,1}}}});
        auto projected=projectRuntimeScene({&backdrop,1},semantic::SemanticGeneration{1});
        list.entries.insert(list.entries.begin(),ReferenceTraversalEntry{projected.records.front(),true,ShapeReferenceCommand{std::get<semantic::ShapeContent>(backdrop.content)}});
    }
    return FramePlan{frame,std::move(list)};
}
void realRoots(){
    for(unsigned dpr:{1U,2U,4U})for(unsigned variant:{0U,1U,2U}){
        RasterSkiaSurfaceProvider target,reference;require(target.resize(256*dpr,128*dpr).code==BackendSubmissionCode::kAccepted,"root resize");
        require(reference.resize(256*dpr,128*dpr).code==BackendSubmissionCode::kAccepted,"reference resize");
        const auto plan=framePlan(1,dpr,variant);SkiaRenderer renderer;RenderCacheOptions options;options.enabled=false;renderer.setRenderCacheOptions(options);
        require(renderer.renderFrame(target,plan).code==BackendSubmissionCode::kAccepted,"canonical real root");
        auto counts=renderer.renderCacheWork();require(counts.executionCpuDecisions>0&&counts.executionDirectCpuTiles>0&&counts.executionGpuTiles==0,"canonical Direct leaf is unaccounted or Raster mislabeled GPU");
        const auto before=counts.executionDirectCpuTiles;EditingOverlay overlay{plan.frame};
        require(renderer.renderTransientOverlay(target,framePlan(2,dpr,variant),&overlay).code==BackendSubmissionCode::kAccepted,"full transient real root");
        counts=renderer.renderCacheWork();require(counts.executionDirectCpuTiles>before&&counts.executionGpuAllocations==0,"transient bypassed common real leaf");
        const auto acquired=reference.acquire();require(acquired.code==SkiaSurfaceAcquireCode::kAcquired,"oracle acquire");
        require(internal::drawReferencePlanToSkCanvas(*acquired.frame.surface->getCanvas(),plan,nullptr,nullptr).code==BackendSubmissionCode::kAccepted,"independent CPU reference");reference.release();
        std::vector<std::uint8_t> actual(256*128*dpr*dpr*4),expected(actual.size());
        require(target.readbackRgba(actual).code==BackendSubmissionCode::kAccepted&&reference.readbackRgba(expected).code==BackendSubmissionCode::kAccepted,"root pixels");
        require(actual==expected,"canonical/full transient differs from CPU reference");
    }
}
void actualOutcomeAndRecovery(){
    auto surface=SkSurfaces::Raster(SkImageInfo::MakeN32Premul(256,128));auto plan=framePlan(3);RenderCacheWork totals;
    {
        internal::RenderExecutionScope scope(internal::ExecutionPurpose::kCanonical,plan,*surface,surface.get(),1,true,4096,totals);
        auto intended=scope.select(*surface->getCanvas(),4,false);intended.route=internal::ExecutionRoute::kGpu;intended.context=&scope;
        scope.allocation(intended,surface.get(),false);
        require(scope.rejected()&&totals.executionGpuAllocations==0&&totals.executionCpuAllocations==1,"GPU intent mislabeled an actual Raster target");
    }
    {
        internal::RenderExecutionScope scope(internal::ExecutionPurpose::kCanonical,plan,*surface,surface.get(),1,true,4096,totals);
        auto intended=scope.select(*surface->getCanvas(),4,false);intended.route=internal::ExecutionRoute::kGpu;intended.context=&scope;
        scope.allocation(intended,nullptr,false);
        require(scope.pending(),"failed supported GPU allocation did not retain explicit Pending");
    }
    internal::CompactCoverageTargets targets;targets.budget=std::make_shared<resources::ResourceBudgetManager>(resources::BudgetLimits{2048,2048});
    auto token=targets.budget->reserve(resources::ResourceClass::kScratch,2048,true);require(bool(token),"pressure reserve");
    {
        internal::RenderExecutionScope scope(internal::ExecutionPurpose::kCanonical,plan,*surface,surface.get(),1,true,4096,totals);
        const auto decision=scope.select(*surface->getCanvas(),4,false);const auto denied=targets.acquire(*surface->getCanvas(),16,16,decision,nullptr);
        require(!denied.surface&&denied.outcome==internal::ExecutionRoute::kPending&&scope.pending(),"temporary pressure silently fell back");
    }
    targets.budget->release(*token);
    {
        internal::RenderExecutionScope scope(internal::ExecutionPurpose::kCanonical,plan,*surface,surface.get(),1,true,4096,totals);
        const auto decision=scope.select(*surface->getCanvas(),4,false);const auto recovered=targets.acquire(*surface->getCanvas(),16,16,decision,nullptr);
        require(bool(recovered.surface)&&!scope.pending()&&!scope.rejected(),"stable demand did not recover");
        const auto impossible=targets.acquire(*surface->getCanvas(),256,256,decision,nullptr);
        require(!impossible.surface&&impossible.outcome==internal::ExecutionRoute::kRejected,"impossible capacity retries forever");
    }
    targets.clear();require(targets.budget->used(resources::ResourceClass::kScratch)==0,"temporary leases leaked");
}

void reuseAndInvalidRoot(){
    RasterSkiaSurfaceProvider target;require(target.resize(256,128).code==BackendSubmissionCode::kAccepted,"reuse target");
    SkiaRenderer renderer;const auto plan=framePlan(1,1,1);
    require(renderer.renderFrame(target,plan).code==BackendSubmissionCode::kAccepted,"cold mixed cache");
    auto before=renderer.renderCacheWork();require(renderer.renderFrame(target,framePlan(2,1,1)).code==BackendSubmissionCode::kAccepted,"warm mixed cache");
    const auto after=renderer.renderCacheWork();
    require(after.executionPixelReuses>before.executionPixelReuses&&after.executionCpuTiles==before.executionCpuTiles,"pixel reuse did not bypass coverage");
    const auto presents=target.presentCount();require(target.advanceGeneration().code==BackendSubmissionCode::kAccepted,"generation advance");
    require(renderer.renderFrame(target,plan).code==BackendSubmissionCode::kRejected&&target.presentCount()==presents,"stale actual root presented");
}

class MisdescribedGpu final:public SkiaSurfaceProvider {
 public:
    RasterSkiaSurfaceProvider actual;
    RenderTargetInfo describe()const noexcept override{auto d=actual.describe();d.kind=RenderTargetKind::kGpuWindow;d.backend=RenderTargetBackend::kWebGL2;d.capabilities.gpuAccelerated=true;return d;}
    SkiaSurfaceAcquireResult acquire()noexcept override{return actual.acquire();}
    void release()noexcept override{actual.release();}
    BackendSubmissionResult present()noexcept override{return actual.present();}
    BackendSubmissionResult resize(std::uint32_t w,std::uint32_t h)noexcept override{return actual.resize(w,h);}
    BackendSubmissionResult readbackRgba(std::span<std::uint8_t> out)noexcept override{return actual.readbackRgba(out);}
    std::uint64_t generation()const noexcept override{return actual.generation();}
    BackendSubmissionResult advanceGeneration()noexcept override{return actual.advanceGeneration();}
    std::uint64_t readbackCount()const noexcept override{return actual.readbackCount();}
    std::uint64_t cpuCopyCount()const noexcept override{return actual.cpuCopyCount();}
    std::uint64_t presentCount()const noexcept override{return actual.presentCount();}
};
void declaredGpuActualRaster(){
    MisdescribedGpu target;require(target.resize(256,128).code==BackendSubmissionCode::kAccepted,"misdescribed target");
    SkiaRenderer renderer;const auto result=renderer.renderFrame(target,framePlan(9));
    std::printf("declared_gpu=1 actual_root_gpu=0 rejected=%d presents=%llu\n",result.code==BackendSubmissionCode::kRejected,static_cast<unsigned long long>(target.presentCount()));
    require(result.code==BackendSubmissionCode::kRejected&&target.presentCount()==0,"GPU provider intent returned Raster but rendered silently on CPU");
}

void staleDecision(){
    auto surface=SkSurfaces::Raster(SkImageInfo::MakeN32Premul(256,128));auto plan=framePlan(4);RenderCacheWork totals;
    internal::CompactCoverageTargets targets;
    internal::RenderExecutionScope scope(internal::ExecutionPurpose::kCanonical,plan,*surface,surface.get(),1,true,4096,totals);
    auto decision=scope.select(*surface->getCanvas(),4,false);++decision.basis.metrics;
    const auto result=targets.acquire(*surface->getCanvas(),16,16,decision,nullptr);
    require(!result.surface&&result.outcome==internal::ExecutionRoute::kRejected,"allocator consumed a stale owner decision");
}

}
int main(int argc,char** argv){try{if(argc>1&&std::string_view(argv[1])=="declared"){declaredGpuActualRaster();return 0;}if(argc>1&&std::string_view(argv[1])=="basis"){staleDecision();return 0;}actualTargetKey();realRoots();actualOutcomeAndRecovery();staleDecision();reuseAndInvalidRoot();declaredGpuActualRaster();return 0;}catch(const std::exception& e){std::fprintf(stderr,"%s\n",e.what());return 1;}}
