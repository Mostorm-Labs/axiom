#pragma once
#include "canvas/render/navigation_sources.hpp"
#include "canvas/render/render_cache.hpp"
#include "canvas/ink/compact_contour.hpp"
#include "canvas/render/preview_surface.hpp"
#include "canvas/render/skia_surface_provider.hpp"
#include "canvas/resources/resource_budget_manager.hpp"
#include "canvas/scheduling/runtime_scheduler.hpp"
#include "canvas/render/tile/tile_grid.hpp"
#include "include/core/SkSurface.h"
#include "include/core/SkImage.h"
#include <map>

namespace canvas::render::internal {
class SkiaNavigationTiles final {
 public:
    NavigationWork work;
    void cancelWork(){cancel();work.pending=false;work.presented=false;}
    BackendSubmissionResult render(SkiaSurfaceProvider&,const CanonicalViewportTransform&,
        const FrameState&,const NavigationSourceQuery&,std::uint64_t resourceEpoch,
        const RenderCacheOptions&,std::shared_ptr<resources::ResourceBudgetManager>);
 private:
    struct Sources {std::uint64_t scene=0;std::vector<ReferenceTraversalEntry> entries;resources::ResourceLease lease;};
    struct Tile {
        std::shared_ptr<const Sources> sources;
        sk_sp<SkImage> image;
        std::uint64_t used=0,checkedScene=0;
        resources::ResourceLease lease;
    };
    struct Job {
        TileKey key;
        std::shared_ptr<const Sources> sources;
        sk_sp<SkSurface> surface,coverage;
        std::size_t source=0,knot=0;
        double distance=0,totalDistance=0;
        std::optional<ink::CompactContourCompiler> contour;
        std::uint64_t id=0;
        resources::ResourceLease lease;
    };
    std::map<TileKey,Tile> tiles_;
    std::map<TileKey,std::shared_ptr<const Sources>> sources_;
    std::unique_ptr<Job> job_;
    TileGrid grid_{256};
    scheduling::RuntimeScheduler scheduler_{4};
    const void* provider_=nullptr;
    std::uint64_t surface_=0,resource_=0,serial_=0,tick_=0;
    std::unique_ptr<FrameState> last_;
    CanonicalViewportTransform camera_{};
    std::uint64_t scene_=0;
    bool dirty_=true;
    void cancel();
};
}
