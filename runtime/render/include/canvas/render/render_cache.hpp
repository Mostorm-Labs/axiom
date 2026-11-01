#pragma once
#include "canvas/render/render_group.hpp"
#include <cstddef>
#include <cstdint>

namespace canvas::render {
struct RenderCacheOptions final {
    bool enabled=true;
    std::size_t maxBytes=32U*1024U*1024U;
    std::size_t maxEntryBytes=8U*1024U*1024U;
    std::size_t maxEntries=512;
    // Candidate world-tile metadata is opt-in until seam/scale benchmarks close.
    bool tileCandidate=false;
    double tileWorldSize=256;
    std::int32_t tileScale=0;
    // Owner work policy for navigation feedback. Not a public SLO.
    std::size_t navigationMaxSteps=64;
};
// Cooperative owner-thread policy, not a frame-latency guarantee. A group is
// atomic; a single expensive group can exceed the time target.
struct RenderPreparationBudget final {
    std::size_t maxGroups=2;
    std::uint64_t maxMicroseconds=4000;
};
struct RenderCacheWork final {
    std::uint64_t hits=0,misses=0,rasterizedGroups=0,directGroups=0,evictions=0;
    std::size_t retainedBytes=0,entries=0;
    std::uint64_t coverageBuilds=0,coverageHits=0,scratchAllocations=0;
    std::size_t coverageBytes=0,scratchPeakBytes=0;
    std::uint64_t tileRequests=0,tileRasters=0,tileHits=0;
    std::uint64_t preparationSlices=0,preparationYields=0;
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
            (cached.transformIndependent || a.transform==b.transform)
            ? RenderSourceKind::kRasterCache : RenderSourceKind::kDirect;
        // External traversal order is applied at composition, never baked in.
    }
};
}
