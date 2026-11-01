#pragma once
#include "canvas/render/render_cache.hpp"
#include "compact_raster_internal.hpp"
#include "canvas/render/render_group_manager.hpp"
#include "canvas/render/frame_plan.hpp"
#include "canvas/render/tile/tile_grid.hpp"
#include "include/core/SkImage.h"
#include "include/core/SkSurface.h"
#include "include/core/SkPicture.h"
#include "canvas/resources/resource_budget_manager.hpp"
#include "canvas/scheduling/runtime_scheduler.hpp"
#include <unordered_map>

namespace canvas::render::internal {
class SkiaRenderCache final {
  public:
    std::shared_ptr<resources::ResourceBudgetManager> budget;
    RenderCacheOptions options{};
    RenderCacheWork work{};
    RenderDamage damage{};
    void begin(const FramePlan&, const void* target, SkSurface&, std::uint64_t resourceEpoch);
    // Called only after full plan validation. False executes permanent Direct.
    bool draw(SkCanvas&,const FramePlan&,const ReferenceTraversalEntry&,bool isolated,bool composite=true);
    enum class Preparation { kReady, kPending, kFallback };
    Preparation prepare(SkCanvas&,const FramePlan&,const RenderPreparationBudget&);
    void clear() noexcept;
    void noteDirect() noexcept {++work.directGroups;++work.rasterizedGroups;}
  private:
    struct Entry {RenderCacheValidity validity;sk_sp<SkImage> image;SkIRect bounds;std::size_t bytes=0;std::uint64_t used=0;resources::ResourceLease lease;};
    std::unordered_map<foundation::ObjectId,Entry,foundation::ObjectIdHash> entries_;
    struct TileEntry {std::uint64_t generation;sk_sp<SkImage> image;SkIRect device;resources::ResourceLease lease;};
    std::map<TileKey,TileEntry> tileEntries_;
    bool tiledFrame_=false;
    foundation::DocumentBackground tileBackground_{};
    foundation::WorldRect tileClip_{};
    bool drawTiles(SkCanvas&,const FramePlan&);
    struct Coverage {RenderGroupRevisions revisions;sk_sp<SkPicture> picture;std::size_t bytes=0;std::uint64_t used=0;resources::ResourceLease lease;};
    std::unordered_map<foundation::ObjectId,Coverage,foundation::ObjectIdHash> coverage_;
    CompactCoverageTargets coverageTargets_;
    struct Pending {foundation::ObjectId id;RenderCacheValidity validity;SkIRect crop;std::unique_ptr<CompactStrokeRasterJob> raster;};
    std::unique_ptr<Pending> pending_;
    struct Admission {SkIRect crop;};
    std::unordered_map<foundation::ObjectId,Admission,foundation::ObjectIdHash> admissions_;
    bool admissionReady_=false;
    bool ensureScratch(SkCanvas&);
    SkMatrix entryMatrix(SkCanvas&,const FramePlan&,const ReferenceTraversalEntry&)const;
    void configureScratch(SkCanvas&,const FramePlan&,const SkMatrix&);
    bool store(SkCanvas&,const ReferenceTraversalEntry&,const RenderCacheValidity&,const SkIRect&,bool composite);
    void noteCoverage(const CompactRasterWork&);
    resources::ResourceLease scratchLease_;
    sk_sp<SkSurface> scratch_;
    std::unique_ptr<TileGrid> tiles_;
    scheduling::RuntimeScheduler scheduler_{8};
    std::uint64_t workId_=0;
    RenderGroupManager groups_;
    const void* target_=nullptr;
    const SkSurface* surface_=nullptr;
    std::unique_ptr<FrameState> frame_;
    WorldToViewAffine transform_{};
    std::uint64_t targetEpoch_=0,tick_=0;
    void evict(std::size_t bytes);
    sk_sp<SkPicture> coverage(const ReferenceTraversalEntry&,const RenderGroup&,const SkRect&);
};
}
