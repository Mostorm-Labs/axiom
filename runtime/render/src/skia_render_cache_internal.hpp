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
#include "source_retention.hpp"
#include "canvas/scheduling/runtime_scheduler.hpp"
#include <unordered_map>

namespace canvas::render::internal {
class SkiaRenderCache final {
  public:
    std::shared_ptr<resources::ResourceBudgetManager> budget;
    std::shared_ptr<SourceRetentionPool> sourcePool;
    std::function<void(std::size_t)> reclaimRecorded;
    std::function<void(std::size_t)> reclaimPreparation;
    RenderCacheOptions options{};
    RenderCacheWork work{};
    RenderDamage damage{};
    void begin(const FramePlan&, const void* target, SkSurface&, std::uint64_t resourceEpoch, foundation::ObjectId activeSource = {});
    // Called only after full plan validation. False executes permanent Direct.
    bool draw(SkCanvas&,const FramePlan&,const ReferenceTraversalEntry&,bool isolated,bool composite=true);
    enum class Preparation { kReady, kPending, kFallback, kResourcePending, kRejected };
    Preparation prepare(SkCanvas&,const FramePlan&,const RenderPreparationBudget&,ImageResourceResolver*,SkiaTextResources*);
    bool compositePreparedFrame(SkCanvas&);
    // Retained ordered compact runs qualify approximate editing only. Exact
    // frames still consume per-object/streamed composition in original order.
    void drawTransientScene(SkCanvas&,const FramePlan&,ImageResourceResolver*,SkiaTextResources*,bool includeInk);
    bool compositeRetainedRun(SkCanvas&,const FramePlan&,std::size_t& index,bool inkPass=false);
    void clear() noexcept;
    // Navigation may reclaim idle exact work buffers, never an active raster job.
    bool releaseIdleWorkBuffers()noexcept;
    void noteDirect() noexcept {++work.directGroups;++work.rasterizedGroups;}
  private:
    SourceRetentions groupSourceLeases_;
    resources::ResourceLease groupMetadataLease_;
    bool sourceAdmission_=true;
    struct RunStamp {foundation::ObjectId id;RenderGroupRevisions revisions;bool operator==(const RunStamp&)const=default;};
    struct RetainedRun {
        std::size_t begin=0,end=0,next=0;
        std::vector<RunStamp> stamps;
        sk_sp<SkSurface> building,prefix;
        sk_sp<SkImage> image,prefixImage;
        resources::ResourceLease lease;
        std::size_t bytes=0;
    };
    std::vector<RetainedRun> runs_;
    foundation::ObjectId activeSource_{};
    void planRuns(SkCanvas&,const FramePlan&);
    void captureRunEntry(const FramePlan&,std::size_t,const sk_sp<SkImage>&,int x=0,int y=0);
    bool validRun(const RetainedRun&,const FramePlan&)const;
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
    enum class AdmissionKind {kRaster,kEmpty,kDirect,kComposite};
    struct Admission {SkIRect crop;AdmissionKind kind=AdmissionKind::kRaster;};
    std::unordered_map<foundation::ObjectId,Admission,foundation::ObjectIdHash> admissions_;
    bool admissionReady_=false,streamFrame_=false,compositeReady_=false;
    sk_sp<SkSurface> composite_;
    resources::ResourceLease compositeLease_;
    foundation::DocumentBackground preparationBackground_{};
    bool preparationTransparent_=false,preparationIsolate_=false;
    void resetPreparation();
    bool ensureComposite(SkCanvas&,const FramePlan&);
    bool acquireWorkSurface(SkCanvas&,sk_sp<SkSurface>&,resources::ResourceLease&,bool mandatory);
    Preparation allocationFailure_=Preparation::kResourcePending;
    void flushPreparation();
    std::size_t preparationCursor_=0;
    bool ensureScratch(SkCanvas&,bool mandatory=false);
    SkMatrix entryMatrix(SkCanvas&,const FramePlan&,const ReferenceTraversalEntry&)const;
    void configureScratch(SkCanvas&,const FramePlan&,const SkMatrix&);
    bool store(SkCanvas&,const ReferenceTraversalEntry&,const RenderCacheValidity&,const SkIRect&,bool composite,bool preservePrepared=false);
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
