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
};
struct RenderCacheWork final {
    std::uint64_t hits=0,misses=0,rasterizedGroups=0,directGroups=0,evictions=0;
    std::size_t retainedBytes=0,entries=0;
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
