#pragma once
#include "canvas/render/render_group.hpp"
#include <cstddef>
#include <cstdint>

namespace canvas::render {
struct RenderCacheOptions final {
    bool enabled=true;
    std::size_t maxBytes=32U*1024U*1024U;
    // Shared derived-resource hard cap, independent of optional cache capacity.
    // Mandatory cooperative work buffers use this same manager, not a new pool.
    std::size_t sharedMaxBytes=96U*1024U*1024U;
    std::size_t maxEntryBytes=8U*1024U*1024U;
    std::size_t maxEntries=512;
    // Candidate world-tile metadata is opt-in until seam/scale benchmarks close.
    bool tileCandidate=false;
    double tileWorldSize=256;
    std::int32_t tileScale=0;
    // Owner work policy for navigation feedback. Not a public SLO.
    std::size_t navigationMaxSteps=64;
    // CPU time alone does not bound queued GPU paths. Coarse masks use CPU
    // Skia coverage; fine GPU contours have a separate per-opportunity quota.
    std::size_t navigationMaxGpuContourPrimitives=128;
    // V3 GPU saturated interior + pinned CPU exact edge; legacy stays CPU.
    bool gpuCoverageCandidate=true;
    // Navigation admission policies, internal only; neither preempts a query
    // nor promises a display deadline. Prewarm is optional and never pins demand.
    std::uint64_t navigationMicroseconds=2000;
    std::size_t navigationMaxQueries=4;
    std::size_t navigationPrewarmTiles=2;
};
// Cooperative owner-thread admission policy, not a frame-latency guarantee.
// Coverage checkpoints bound work inside an individual compact stroke.
struct RenderPreparationBudget final {
    std::size_t maxGroups=2;
    std::uint64_t maxMicroseconds=8000;
    // Checkpoints cover only 32 knots. A small fixed ceiling forced cheap
    // work into dozens of RAFs before the time allowance was exhausted.
    // Retain a finite safety ceiling; callers may still request one-step work.
    std::size_t maxCoverageSteps=4096;
};
struct RenderCacheWork final {
    // Actual destination area admitted for continuous local-erase feedback.
    std::uint64_t eraseFullFrames=0,eraseDamageFrames=0,eraseDamagePixels=0;
    std::uint64_t hits=0,misses=0,rasterizedGroups=0,directGroups=0,evictions=0;
    std::size_t retainedBytes=0,entries=0;
    std::uint64_t coverageBuilds=0,coverageHits=0,scratchAllocations=0;
    std::uint64_t gpuCoverageTiles=0,cpuCoverageTiles=0,coverageTargetAllocations=0,coverageTargetReuses=0,preparationCoverageSteps=0;
    std::uint64_t gpuInteriorPixels=0,cpuEdgePixels=0;
    std::size_t coverageBytes=0,scratchPeakBytes=0;
    std::uint64_t tileRequests=0,tileRasters=0,tileHits=0;
    std::uint64_t preparationSlices=0,preparationYields=0;
    std::uint64_t retainedRunBuilds=0,retainedRunHits=0;
    std::size_t retainedRunBytes=0;
    std::uint64_t preparationDirectEntries=0,preparationBudgetEntries=0;
};
enum class RenderSourceKind { kDirect, kRasterCache };
struct RenderCacheValidity final {
    RenderGroupRevisions revisions{};
    std::uint64_t targetEpoch=0;
    bool transformIndependent=false;
};
// Validity policy is backend-neutral; representation/handles stay private.
class RenderSourceResolver final {
  public:
    [[nodiscard]] static RenderSourceKind resolve(const RenderCacheValidity& cached,
        const RenderCacheValidity& current) noexcept {
        const auto& a=cached.revisions;const auto& b=current.revisions;
        return cached.targetEpoch==current.targetEpoch && a.content==b.content &&
            a.clip==b.clip && a.effect==b.effect && a.resource==b.resource &&
            a.geometry==b.geometry &&
            (cached.transformIndependent || a.transform==b.transform)
            ? RenderSourceKind::kRasterCache : RenderSourceKind::kDirect;
        // External traversal order is applied at composition, never baked in.
    }
};
}
