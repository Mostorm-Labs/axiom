#pragma once
#include "canvas/render/frame_plan.hpp"
#include "canvas/render/render_cache.hpp"
#include "canvas/render/render_target.hpp"
#include <cstddef>
#include <cstdint>

class SkCanvas;
class SkSurface;
namespace canvas::render::internal {
class CompactCoverageTargets;
enum class ExecutionPurpose { kCanonical, kFullTransientEdit, kCompatibility };
enum class ExecutionFidelity { kExact, kFeedback };
enum class ExecutionRoute { kCpu, kGpu, kPending, kRejected };
enum class ExecutionReason { kRasterRoot, kOwnerCpuPolicy, kGpuV4, kRangeCompatibility,
    kLegacyCompatibility, kWrongContext, kInvalidGeometry, kAllocationDenied,
    kTargetMismatch, kMissingCompletion, kStaleBasis, kImpossibleCapacity, kPixelReuse };
struct ExecutionBasis final {
    const void* target=nullptr;
    std::uint64_t view=0,semantic=0,scene=0,frame=0,surface=0,metrics=0,resource=0;
    bool operator==(const ExecutionBasis&) const = default;
};
struct ExecutionDecision final {
    ExecutionRoute route=ExecutionRoute::kCpu;
    ExecutionReason reason=ExecutionReason::kLegacyCompatibility;
    ExecutionFidelity fidelity=ExecutionFidelity::kExact;
    unsigned composition=0;
    const void* context=nullptr;
    ExecutionBasis basis{};
    std::size_t maximumTargetBytes=0;
    bool allowCpuFallback=true;
    bool ownerScoped=false;
};
// Existing Render owner invocation binding. It is bounded stack state; no
// document/cache/worker owner and no event buffer. Legacy dispatch wrappers
// consult the same typed decision at their real compact leaf.
class RenderExecutionScope final {
 public:
    RenderExecutionScope(ExecutionPurpose,const FramePlan&,SkSurface&,
        const void* provider,std::uint64_t resource,bool gpuEnabled,
        std::size_t capacity,RenderCacheWork&,RenderTargetKind declared=RenderTargetKind::kExternal) noexcept;
    ~RenderExecutionScope();
    RenderExecutionScope(const RenderExecutionScope&)=delete;
    RenderExecutionScope& operator=(const RenderExecutionScope&)=delete;
    static RenderExecutionScope* current() noexcept;
    void bindTargets(CompactCoverageTargets& targets) noexcept {targets_=&targets;}
    void unbindTargets() noexcept {targets_=nullptr;}
    CompactCoverageTargets* targets()const noexcept{return targets_;}
    ExecutionDecision select(SkCanvas&,std::uint32_t algorithm,bool range) noexcept;
    bool accepts(const ExecutionDecision&)const noexcept;
    void allocation(const ExecutionDecision&,SkSurface*,bool reused) noexcept;
    void coverage(SkSurface&,bool direct) noexcept;
    void unavailable(const ExecutionDecision&,ExecutionReason,bool terminal=false) noexcept;
    void reusePixels() noexcept;
    void sourceWork(std::size_t support,std::size_t length,std::size_t compile=0,std::size_t replay=0) noexcept {
        totals_.bodySupportKnots+=support;totals_.bodyLengthInputs+=length;totals_.bodyCompileInputs+=compile;totals_.bodyReplayInputs+=replay;
    }
    void preparedUse(bool direct) noexcept {if(direct)++totals_.preparedDirectDraws;else ++totals_.preparedCanonicalDraws;}
    bool pending()const noexcept{return pending_;}
    bool rejected()const noexcept{return rejected_;}
 private:
    RenderExecutionScope* previous_=nullptr;
    [[maybe_unused]] ExecutionPurpose purpose_;
    RenderTargetKind declaredKind_=RenderTargetKind::kExternal;
    ExecutionFidelity fidelity_=ExecutionFidelity::kExact;
    unsigned composition_=0;
    ExecutionBasis basis_;
    const void* rootContext_=nullptr;
    bool gpuEnabled_=false,pending_=false,rejected_=false;
    std::size_t capacity_=0;
    CompactCoverageTargets* targets_=nullptr;
    RenderCacheWork& totals_;
    ExecutionReason reason_=ExecutionReason::kLegacyCompatibility;
    std::uint64_t gpu_=0,cpu_=0,directGpu_=0,directCpu_=0,reuse_=0;
    std::uint64_t beforeCanonicalDraws_=0,beforeDirectDraws_=0;
    std::uint64_t beforeGpuAllocations_=0,beforeCpuAllocations_=0,beforeTargetReuses_=0;
};
void copyExecutionCounters(RenderCacheWork&,const RenderCacheWork&) noexcept;
}
