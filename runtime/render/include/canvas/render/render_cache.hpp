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
    // CPU time alone does not bound queued GPU paths. Fine contours and
    // qualified masked coarse contours have separate submission ceilings.
    std::size_t navigationMaxGpuContourPrimitives=128;
    // Masked v4 bootstrap uses qualified SrcOver GPU coverage. Keep its own
    // finite command ceiling; the same owner time/step/memory bounds apply.
    std::size_t navigationMaxGpuCoarseContourPrimitives=1024;
    // V4 pinned contour with GPU AA (no CPU edge correction); V3 retains
    // its saturated-interior/exact-edge hybrid; legacy stays CPU.
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
    std::uint64_t preparedCanonicalDraws=0,preparedDirectDraws=0,preparedCanonicalQueries=0,preparedDirectQueries=0;
    std::uint64_t preparedJobAdmissions=0,preparedAdvances=0,preparedSupportKnots=0,preparedDistanceInputs=0,preparedCompileInputs=0,preparedPathPrimitives=0,preparedMaxAdvanceKnots=0;
    std::uint64_t bodySupportKnots=0,bodyLengthInputs=0,bodyCompileInputs=0,bodyReplayInputs=0;
    // Current renderer-owned prepared service facts, not SDK/public diagnostics.
    std::uint64_t preparedBodyBuilds=0,preparedBodyHits=0,preparedIdentityHits=0,preparedHashBytes=0,preparedContentComparisons=0;
    std::uint64_t preparedIndexQueries=0,preparedNavigationQueries=0,preparedInkQueries=0,preparedSourceFootprintEvaluations=0;
    std::size_t preparedAliases=0,preparedIndexReservationBytes=0;
    // Private Render execution facts: actual targets/Direct are not inferred
    // from a requested-compatible bool or the parent GPU context.
    std::uint64_t executionRasterRoot=0,executionPolicyCpu=0,executionRangeCpu=0,executionLegacyCpu=0;
    std::uint64_t executionGpuDecisions=0,executionCpuDecisions=0,executionCompatibility=0;
    std::uint64_t executionGpuAllocations=0,executionCpuAllocations=0,executionTargetReuses=0;
    std::uint64_t executionGpuTiles=0,executionCpuTiles=0,executionDirectGpuTiles=0,executionDirectCpuTiles=0;
    std::uint64_t executionPending=0,executionRejected=0,executionAllocationDenials=0,executionTargetMismatches=0,executionPixelReuses=0;
    // Actual destination area admitted for continuous local-erase feedback.
    std::uint64_t eraseFullFrames=0,eraseDamageFrames=0,eraseDamagePixels=0;
    std::uint64_t bodyTileBuilds=0,bodyTileHits=0,maskTileUpdates=0,maskPrimitiveDraws=0,maskTileResets=0;
    std::size_t inkLayerBytes=0;
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
