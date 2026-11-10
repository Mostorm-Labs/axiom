#pragma once
#include "canvas/render/render_cache.hpp"
#include "compact_raster_internal.hpp"
#include "canvas/render/render_group_manager.hpp"
#include "canvas/render/frame_plan.hpp"
#include "canvas/render/editing_overlay.hpp"
#include "canvas/render/tile/tile_grid.hpp"
#include "include/core/SkImage.h"
#include "include/core/SkSurface.h"
#include "include/core/SkPicture.h"
#include "canvas/resources/resource_budget_manager.hpp"
#include "source_retention.hpp"
#include "skia_ink_layers.hpp"
#include "canvas/scheduling/runtime_scheduler.hpp"
#include <unordered_map>

namespace canvas::render::internal {
// Private qualification counters. Updates and observation APIs are compiled
// out of normal consumers; this value is never part of a consumer DTO.
struct ObjectInteractionWork final {
    std::uint64_t builds=0,hits=0,invalidations=0,fullSceneDraws=0,activeDraws=0;
    std::uint64_t retries=0,gpuLeaseMarks=0,denials=0;
    std::size_t leaseBytes=0;
};
// Immutable completed pixels shared by exact presentation and navigation.
// Both aliases retain this one charged allocation; no framebuffer recapture.
struct PreparedFrameImage final {
    sk_sp<SkImage> image;
    resources::ResourceLease pixels,metadata;
    void markGpuUse()const noexcept{(void)pixels.markGpuUse();}
};
class SkiaRenderCache final {
  public:
    std::shared_ptr<resources::ResourceBudgetManager> budget;
    std::shared_ptr<SourceRetentionPool> sourcePool;
    std::shared_ptr<PreparedGeometryCache> preparedGeometry;
    std::shared_ptr<EraseMaskPathCache> maskHistory;
    std::function<void(std::size_t)> reclaimRecorded;
    std::function<void(std::size_t)> reclaimPreparation;
    RenderCacheOptions options{};
    RenderCacheWork work{};
    RenderDamage damage{};
    ObjectInteractionWork objectWork{};
    void begin(const FramePlan&, const void* target, SkSurface&, std::uint64_t resourceEpoch, foundation::ObjectId activeSource = {},bool transient=false);
    CompactCoverageTargets& executionTargets() noexcept {coverageTargets_.budget=budget;return coverageTargets_;}
    bool canRetainCompact(const SkCanvas&,const ReferenceTraversalEntry&) const noexcept;
    // Called only after full plan validation. False executes permanent Direct.
    bool draw(SkCanvas&,const FramePlan&,const ReferenceTraversalEntry&,bool isolated,bool composite=true,bool allowBuild=true);
    enum class Preparation { kReady, kPending, kFallback, kResourcePending, kRejected };
    Preparation prepare(SkCanvas&,const FramePlan&,const RenderPreparationBudget&,ImageResourceResolver*,SkiaTextResources*,PreparationAllowance* shared=nullptr);
    Preparation prepareBodies(SkCanvas&,const FramePlan&,PreparationAllowance&);
    Preparation prepareWorkingSet(SkCanvas&,const FramePlan&,PreparationAllowance&,ImageResourceResolver*,SkiaTextResources*,bool direct,const EditingOverlay* overlay=nullptr);
    bool stagedBodies()const noexcept{return stagedBodies_;}
    bool needsPreparedBody(const ReferenceTraversalEntry&)const noexcept;
    Preparation prepareBody(const ReferenceTraversalEntry&,PreparationAllowance&);
    std::shared_ptr<const PreparedNavigationGeometry> readyBody(const ReferenceTraversalEntry& e,PreparedConsumer c){
        if(!needsPreparedBody(e))return nullptr;
        auto discovered=preparedGeometry->lookup(e,c);
        const auto found=bodyDependencies_.find(e.record.objectId);
        if(found!=bodyDependencies_.end()&&sameBodyIdentity(found->second.identity,preparedBodyIdentity(e.command))&&
           found->second.body&&found->second.body->ready())return found->second.body;
        return discovered;
    }
    bool hasPixels(const ReferenceTraversalEntry&)const;
    bool compositePreparedFrame(SkCanvas&);
    [[nodiscard]] std::shared_ptr<const PreparedFrameImage> completedFrame()const noexcept{return preparedFrame_;}
    // Retained ordered compact runs qualify approximate editing only. Exact
    // frames still consume per-object/streamed composition in original order.
    bool drawTransientScene(SkCanvas&,const FramePlan&,ImageResourceResolver*,SkiaTextResources*,bool includeInk,bool damageOnly=false,const CompactDeviceGrid* grid=nullptr,bool objectTransform=false);
    [[nodiscard]] const RenderGroupManager::EraseDamage& eraseDamage()const noexcept{return eraseDamage_;}
    void acknowledgeTransient() noexcept {pendingErase_={};hasPendingErase_=false;}
    bool acquireTransientSurface(SkCanvas& canvas,sk_sp<SkSurface>& surface,resources::ResourceLease& lease){return acquireWorkSurface(canvas,surface,lease,true,resources::ResourceClass::kCanonicalFrame);}
    [[nodiscard]] std::uint64_t frameSequence()const noexcept{return tick_;}
    bool prepareEraseDamage(SkCanvas& canvas,bool mandatory=false){return (sourceAdmission_||mandatory)&&ensureScratch(canvas,mandatory);}
    enum class InkDraw {kSkipped,kDrawn,kRetry};
    InkDraw drawDamagedInk(SkCanvas&,const FramePlan&,const ReferenceTraversalEntry&,ImageResourceResolver*,SkiaTextResources*,const CompactDeviceGrid*);
    bool compositeRetainedRun(SkCanvas&,const FramePlan&,std::size_t& index,bool inkPass=false);
    void drawHistoricalMasks(SkCanvas&,const ReferenceTraversalEntry&);
    void clear() noexcept;
    // Entering live erasure changes the useful representation. Retire old
    // complete masked sources before the next GPU epoch pins their leases.
    void releaseMaterialSources(bool releaseWorkSurfaces=true)noexcept;
    // Keep matching exact work resumable; obsolete camera/target work cannot
    // reserve the buffers needed by a renewed navigation demand.
    bool releaseIdleWorkBuffers(const FrameState&,const WorldToViewAffine&,
        const void* target,std::uint64_t resourceEpoch)noexcept;
    void noteDirect() noexcept {++work.directGroups;++work.rasterizedGroups;}
  private:
    struct StaticSceneStamp final {
        foundation::ObjectId id{};
        RenderGroupRevisions revisions{};
        float opacity=1.0F;
        bool contributes=false;
        foundation::WorldRect visualBounds{},worldBounds{};
        bool operator==(const StaticSceneStamp&) const = default;
    };
    struct StaticSceneCache final {
        const void* target=nullptr;
        const void* context=nullptr;
        const SkSurface* surface=nullptr;
        std::uint64_t resourceEpoch=0,targetEpoch=0;
        FrameState frame{};
        WorldToViewAffine transform{};
        foundation::DocumentBackground background{};
        bool transparent=false,isolate=false,includeInk=true;
        foundation::ObjectId active{};
        std::size_t activeIndex=0,activeEnd=0;
        std::vector<StaticSceneStamp> stamps;
        sk_sp<SkImage> prefix,suffix;
        resources::ResourceLease pixelLease,metadataLease;
        std::size_t leaseBytes=0;
        StaticSceneCache(const void* t,const void* c,std::uint64_t resource,
            std::uint64_t epoch, const FrameState& f,const WorldToViewAffine& x,
            const foundation::DocumentBackground& bg,bool clear,bool isolate,
            bool ink,foundation::ObjectId id,std::size_t index)
            :target(t),context(c),resourceEpoch(resource),targetEpoch(epoch),frame(f),
             transform(x),background(bg),transparent(clear),isolate(isolate),includeInk(ink),
             active(id),activeIndex(index) {}
        bool valid() const noexcept { return prefix != nullptr; }
    };
    std::shared_ptr<StaticSceneCache> staticScene_;
    void invalidateStaticScene(bool count=true) noexcept;
    bool staticSceneBasisMatches(const FramePlan&,std::size_t,std::size_t) const noexcept;
    bool buildStaticScene(SkCanvas&,const FramePlan&,std::size_t,std::size_t,ImageResourceResolver*,SkiaTextResources*,const CompactDeviceGrid*);
    struct BodyDependency {
        ImmutableSourceIdentity identity;
        std::shared_ptr<const PreparedNavigationGeometry> body;
        resources::ResourceLease metadata;
        std::uint64_t seen=0;
    };
    std::map<foundation::ObjectId,BodyDependency> bodyDependencies_;
    bool stagedBodies_=false;
    // An active mask may reuse resident immutable bodies without inheriting
    // the previous whole-frame pixel working-set admission failure.
    bool movingEraseBodies_=false;
    std::size_t stagedDemand_=0;
    std::optional<semantic::ObjectRecord> stagedCreation_;
    std::optional<scene::ConnectorGeometry> stagedCreationRoute_;
    resources::ResourceLease stagedCreationLease_;
    static bool sameBodyIdentity(const ImmutableSourceIdentity& a,const ImmutableSourceIdentity& b){
        const ImmutableSourceIdentityLess less;return !less(a,b)&&!less(b,a);
    }
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
    SkiaInkLayers inkLayers_;
    struct Pending {foundation::ObjectId id;RenderCacheValidity validity;SkIRect crop;std::unique_ptr<CompactStrokeRasterJob> raster;};
    std::unique_ptr<Pending> pending_;
    enum class AdmissionKind {kRaster,kEmpty,kDirect,kComposite};
    struct Admission {SkIRect crop;AdmissionKind kind=AdmissionKind::kRaster;};
    std::unordered_map<foundation::ObjectId,Admission,foundation::ObjectIdHash> admissions_;
    bool admissionReady_=false,streamFrame_=false,compositeReady_=false;
    sk_sp<SkSurface> composite_;
    resources::ResourceLease compositeLease_;
    std::shared_ptr<const PreparedFrameImage> preparedFrame_;
    foundation::DocumentBackground preparationBackground_{};
    bool preparationTransparent_=false,preparationIsolate_=false;
    void resetPreparation();
    bool ensureComposite(SkCanvas&,const FramePlan&);
    bool finishComposite();
    bool acquireWorkSurface(SkCanvas&,sk_sp<SkSurface>&,resources::ResourceLease&,bool mandatory,
        resources::ResourceClass kind=resources::ResourceClass::kScratch);
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
    RenderGroupManager::EraseDamage eraseDamage_;
    RenderGroupManager::EraseDamage pendingErase_;bool hasPendingErase_=false;
    bool damageOnly_=false,damageFailed_=false;
    const void* target_=nullptr;
    const SkSurface* surface_=nullptr;
    std::unique_ptr<FrameState> frame_;
    WorldToViewAffine transform_{};
    std::uint64_t targetEpoch_=0,tick_=0,resourceEpoch_=0;
    void evict(std::size_t bytes);
    sk_sp<SkPicture> coverage(const ReferenceTraversalEntry&,const RenderGroup&,const SkRect&);
};
}
