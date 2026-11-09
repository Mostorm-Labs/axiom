#pragma once
#include "canvas/render/navigation_sources.hpp"
#include "canvas/render/render_cache.hpp"
#include "canvas/ink/compact_contour.hpp"
#include "canvas/render/preview_surface.hpp"
#include "canvas/render/skia_surface_provider.hpp"
#include "canvas/render/skia_text_resources.hpp"
#include "canvas/render/image_resource.hpp"
#include "skia_prepared_geometry.hpp"
#include "navigation_pixel_validity.hpp"
#include "gpu_resource_retirement.hpp"
#include "canvas/resources/resource_budget_manager.hpp"
#include "canvas/scheduling/runtime_scheduler.hpp"
#include "canvas/render/tile/tile_grid.hpp"
#include "include/core/SkSurface.h"
#include "include/core/SkImage.h"
#include <map>
#include <atomic>
#include <unordered_map>
#include <array>

class SkCanvas;
namespace canvas::render::internal {
// Invocation-scoped display source, validated by the renderer against the full
// frame/provider stamp. Outside bounds requires a Runtime background proof.
struct RetainedNavigationSource final {
    foundation::WorldRect bounds{};
    std::span<const foundation::WorldRect> regions{};
    double density=0;
    std::function<bool()> extend;
    std::function<void(SkCanvas&,const foundation::WorldRect&)> draw;
    const RetainedNavigationSource* fallback=nullptr;
};
class SkiaNavigationTiles final {
 public:
    NavigationWork work;
    void cancelWork(){cancel();work.pending=false;work.presented=false;}
    void reclaimForExact(const std::shared_ptr<resources::ResourceBudgetManager>&,std::size_t limit,std::size_t requested,
        const FrameState&,const SkiaSurfaceProvider*,std::uint64_t resourceEpoch,bool mandatory=false);
    BackendSubmissionResult render(SkiaSurfaceProvider&,const CanonicalViewportTransform&,
        const FrameState&,const NavigationSourceQuery&,std::uint64_t resourceEpoch,
        const RenderCacheOptions&,std::shared_ptr<resources::ResourceBudgetManager>,bool allowPrewarm=false,
        const RetainedNavigationSource* retained=nullptr, SkiaTextResources* textResources=nullptr,
        ImageResourceResolver* images=nullptr,
        const std::function<void(SkCanvas&)>& drawOverlay={},std::shared_ptr<SourceRetentionPool> sourcePool={},std::shared_ptr<GpuResourceRetirement> retirements={},std::shared_ptr<PreparedGeometryCache> prepared={},std::uint32_t background=0xffffffffU);
 private:
    using ValidRegions=NavigationRegions;
    struct Sources {
        std::uint64_t scene=0;
        std::vector<ReferenceTraversalEntry> entries;
        std::vector<std::shared_ptr<const PreparedNavigationGeometry>> geometry;
        resources::ResourceLease lease;
        SourceRetentions sourceLeases;
        std::size_t admittedBytes=0;
        bool stamped=false;
    };
    struct Tile {
        std::shared_ptr<const Sources> sources;
        sk_sp<SkImage> image;
        std::uint64_t used=0,checkedScene=0;
        resources::ResourceLease lease;
        ValidRegions valid;
    };
    struct Job {
        TileKey key;
        std::shared_ptr<const Sources> sources;
        sk_sp<SkSurface> surface,coverage;
        std::size_t source=0,knot=0,primitive=0;
        bool sourceInitialized=false;
        double distance=0,totalDistance=0;
        std::optional<ink::CompactContourCompiler> contour;
        std::shared_ptr<const PreparedNavigationGeometry> geometry;
        std::uint64_t id=0,steps=0;
        bool prewarm=false;
        bool coveragePriority=false;
        foundation::WorldRect demand{};
        bool cpuCoverage=false,gpuCoarse=false;
        resources::ResourceLease lease;
        ValidRegions valid;
    };
    bool usefulFallback(TileKey,const Tile&,const foundation::WorldRect&,int level,std::uint64_t scene)const;
    static constexpr std::size_t tileMetadataBytes=sizeof(Tile)+sizeof(Job);
    std::map<TileKey,Tile> tiles_;
    std::map<TileKey,std::shared_ptr<const Sources>> sources_;
    std::shared_ptr<PreparedGeometryCache> geometry_;
    std::unique_ptr<Job> job_;
    std::unique_ptr<Job> refinement_;
    TileGrid grid_{256};
    scheduling::RuntimeScheduler scheduler_{4};
    std::weak_ptr<resources::ResourceBudgetManager> budgetOwner_;
    const void* provider_=nullptr;
    std::uint64_t surface_=0,resource_=0,serial_=0,tick_=0;
    std::unique_ptr<FrameState> last_;
    std::optional<foundation::WorldRect> fallbackFocus_;
    CanonicalViewportTransform camera_{};
    std::uint64_t scene_=0;
    std::uint32_t background_=0xffffffffU;
    bool dirty_=true;
    std::optional<int> level_;
    std::size_t warmed_=0;
    std::shared_ptr<GpuResourceRetirement> gpuRetirements_;
    struct Patch {TileKey source;foundation::WorldRect area;double density=0;const RetainedNavigationSource* retained=nullptr;};
    bool coverage(const foundation::WorldRect&,std::uint64_t scene,std::vector<Patch>&,
        const RetainedNavigationSource*,std::vector<foundation::WorldRect>* missing=nullptr);
    void compose(SkCanvas&,std::span<const Patch>,const CanonicalViewportTransform&,const FrameState&);
    void cancel();
    void cancelJob(std::unique_ptr<Job>&);
};
}
