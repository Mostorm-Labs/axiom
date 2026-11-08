#include "render_execution.hpp"
#include "include/core/SkCanvas.h"
#include "include/core/SkSurface.h"
#include <cstdio>

namespace canvas::render::internal {
namespace { thread_local RenderExecutionScope* active=nullptr; }
RenderExecutionScope* RenderExecutionScope::current() noexcept{return active;}
RenderExecutionScope::RenderExecutionScope(ExecutionPurpose purpose,const FramePlan& plan,
    SkSurface& target,const void* provider,std::uint64_t resource,bool enabled,
    std::size_t capacity,RenderCacheWork& totals,RenderTargetKind declared) noexcept
    :previous_(active),purpose_(purpose),declaredKind_(declared),basis_{provider,plan.frame.viewId.value(),
      plan.frame.sceneGeneration.value(),plan.frame.sceneReadToken.value(),plan.frame.frameId.value(),
      plan.frame.surfaceGeneration.value(),plan.frame.metricsGeneration.value(),resource},
      rootContext_(target.recordingContext()),gpuEnabled_(enabled),capacity_(capacity),totals_(totals){
    fidelity_=purpose==ExecutionPurpose::kCanonical?ExecutionFidelity::kExact:ExecutionFidelity::kFeedback;
    composition_=unsigned(plan.background.mode)*4U+(plan.transparentBackground?1U:0U)+(plan.isolateEraseMasks?2U:0U);
    beforeGpuAllocations_=totals.executionGpuAllocations;beforeCpuAllocations_=totals.executionCpuAllocations;beforeTargetReuses_=totals.executionTargetReuses;
    active=this;
    const bool claimedGpu=declaredKind_==RenderTargetKind::kGpuWindow||declaredKind_==RenderTargetKind::kGpuOffscreen;
    if((claimedGpu&&!rootContext_)||(declaredKind_==RenderTargetKind::kCpuRaster&&rootContext_)){
        ExecutionDecision mismatch;unavailable(mismatch,ExecutionReason::kTargetMismatch,true);
    }
}
RenderExecutionScope::~RenderExecutionScope(){
    active=previous_;
#if defined(AXIOM_ENABLE_FRAME_TIMING)
    std::fprintf(stderr,"AXIOM_RENDER_EXECUTION {\"version\":1,\"purpose\":%u,\"fidelity\":%u,\"composition\":%u,\"reason\":%u,\"view\":\"%llu\",\"frame\":\"%llu\",\"semantic\":\"%llu\",\"scene\":\"%llu\",\"surface\":\"%llu\",\"metrics\":\"%llu\",\"resource\":\"%llu\",\"declared_kind\":%u,\"actual_root_gpu\":%s,\"gpu_intent\":%s,\"gpu_allocations\":%llu,\"cpu_allocations\":%llu,\"target_reuses\":%llu,\"actual_gpu_tiles\":%llu,\"actual_cpu_tiles\":%llu,\"direct_gpu_tiles\":%llu,\"direct_cpu_tiles\":%llu,\"reuse\":%llu,\"pending\":%s,\"rejected\":%s}\n",
        unsigned(purpose_),unsigned(fidelity_),composition_,unsigned(reason_),static_cast<unsigned long long>(basis_.view),static_cast<unsigned long long>(basis_.frame),
        static_cast<unsigned long long>(basis_.semantic),static_cast<unsigned long long>(basis_.scene),static_cast<unsigned long long>(basis_.surface),static_cast<unsigned long long>(basis_.metrics),static_cast<unsigned long long>(basis_.resource),
        unsigned(declaredKind_),rootContext_?"true":"false",
        (rootContext_||declaredKind_==RenderTargetKind::kGpuWindow||declaredKind_==RenderTargetKind::kGpuOffscreen)&&gpuEnabled_?"true":"false",static_cast<unsigned long long>(totals_.executionGpuAllocations-beforeGpuAllocations_),static_cast<unsigned long long>(totals_.executionCpuAllocations-beforeCpuAllocations_),static_cast<unsigned long long>(totals_.executionTargetReuses-beforeTargetReuses_),
        static_cast<unsigned long long>(gpu_),static_cast<unsigned long long>(cpu_),static_cast<unsigned long long>(directGpu_),static_cast<unsigned long long>(directCpu_),static_cast<unsigned long long>(reuse_),pending_?"true":"false",rejected_?"true":"false");
#endif
}
ExecutionDecision RenderExecutionScope::select(SkCanvas& canvas,std::uint32_t algorithm,bool range) noexcept {
    ExecutionDecision d;d.basis=basis_;d.maximumTargetBytes=capacity_;d.allowCpuFallback=false;d.ownerScoped=true;
    if(rejected_||pending_){d.route=rejected_?ExecutionRoute::kRejected:ExecutionRoute::kPending;d.reason=reason_;d.fidelity=fidelity_;d.composition=composition_;return d;}
    if(!canvas.getTotalMatrix().isFinite()){
        d.route=ExecutionRoute::kRejected;d.reason=ExecutionReason::kInvalidGeometry;
    }else if(range||algorithm!=4||canvas.getTotalMatrix().hasPerspective()){
        d.route=ExecutionRoute::kCpu;d.reason=range?ExecutionReason::kRangeCompatibility:ExecutionReason::kLegacyCompatibility;
        d.allowCpuFallback=true;++totals_.executionCompatibility;
    }else if(!rootContext_){d.route=ExecutionRoute::kCpu;d.reason=ExecutionReason::kRasterRoot;}
    else if(!gpuEnabled_){d.route=ExecutionRoute::kCpu;d.reason=ExecutionReason::kOwnerCpuPolicy;}
    else if(canvas.recordingContext()!=rootContext_){d.route=ExecutionRoute::kRejected;d.reason=ExecutionReason::kWrongContext;}
    else if(!targets_){d.route=ExecutionRoute::kPending;d.reason=ExecutionReason::kAllocationDenied;}
    else{d.route=ExecutionRoute::kGpu;d.reason=ExecutionReason::kGpuV4;d.context=rootContext_;}
    reason_=d.reason;d.fidelity=fidelity_;d.composition=composition_;
    if(d.reason==ExecutionReason::kRasterRoot)++totals_.executionRasterRoot;
    if(d.reason==ExecutionReason::kOwnerCpuPolicy)++totals_.executionPolicyCpu;
    if(d.reason==ExecutionReason::kRangeCompatibility)++totals_.executionRangeCpu;
    if(d.reason==ExecutionReason::kLegacyCompatibility)++totals_.executionLegacyCpu;
    if(d.route==ExecutionRoute::kGpu)++totals_.executionGpuDecisions;
    else if(d.route==ExecutionRoute::kCpu)++totals_.executionCpuDecisions;
    else unavailable(d,d.reason,d.route==ExecutionRoute::kRejected);
    return d;
}
bool RenderExecutionScope::accepts(const ExecutionDecision& d)const noexcept {
    return d.basis==basis_&&d.fidelity==fidelity_&&d.composition==composition_;
}
void RenderExecutionScope::allocation(const ExecutionDecision& d,SkSurface* surface,bool reused) noexcept {
    if(!surface){unavailable(d,ExecutionReason::kAllocationDenied,false);return;}
    if(!reused){if(surface->recordingContext())++totals_.executionGpuAllocations;
        else ++totals_.executionCpuAllocations;}
    if(reused)++totals_.executionTargetReuses;
    if(d.route==ExecutionRoute::kGpu&&surface->recordingContext()!=d.context){
        if(d.allowCpuFallback)++totals_.executionTargetMismatches;
        else unavailable(d,ExecutionReason::kTargetMismatch,true);
    }
}
void RenderExecutionScope::coverage(SkSurface& surface,bool direct) noexcept {
    if(surface.recordingContext()){++gpu_;++totals_.executionGpuTiles;if(direct){++directGpu_;++totals_.executionDirectGpuTiles;}}
    else{++cpu_;++totals_.executionCpuTiles;if(direct){++directCpu_;++totals_.executionDirectCpuTiles;}}
}
void RenderExecutionScope::unavailable(const ExecutionDecision&,ExecutionReason reason,bool terminal) noexcept {
    reason_=reason;
    if(terminal){rejected_=true;++totals_.executionRejected;}else{pending_=true;++totals_.executionPending;}
    if(reason==ExecutionReason::kTargetMismatch||reason==ExecutionReason::kWrongContext||reason==ExecutionReason::kStaleBasis)++totals_.executionTargetMismatches;
    if(reason==ExecutionReason::kAllocationDenied)++totals_.executionAllocationDenials;
}
void RenderExecutionScope::reusePixels() noexcept{reason_=ExecutionReason::kPixelReuse;++reuse_;++totals_.executionPixelReuses;}
void copyExecutionCounters(RenderCacheWork& out,const RenderCacheWork& in) noexcept {
#define COPY(name) out.name=in.name
    COPY(executionRasterRoot);COPY(executionPolicyCpu);COPY(executionRangeCpu);COPY(executionLegacyCpu);
    COPY(executionGpuDecisions);COPY(executionCpuDecisions);COPY(executionCompatibility);
    COPY(executionGpuAllocations);COPY(executionCpuAllocations);COPY(executionTargetReuses);
    COPY(executionGpuTiles);COPY(executionCpuTiles);COPY(executionDirectGpuTiles);COPY(executionDirectCpuTiles);
    COPY(executionPending);COPY(executionRejected);COPY(executionAllocationDenials);COPY(executionTargetMismatches);COPY(executionPixelReuses);
#undef COPY
}
}
