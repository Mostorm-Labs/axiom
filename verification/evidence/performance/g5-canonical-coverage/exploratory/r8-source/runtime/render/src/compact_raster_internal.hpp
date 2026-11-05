#pragma once
#include "canvas/render/skia_scene_renderer.hpp"
#include "canvas/resources/resource_budget_manager.hpp"
#include "canvas/render/render_cache.hpp"
#include "include/core/SkPicture.h"
#include "include/core/SkSurface.h"
#include "include/core/SkMatrix.h"
#include "include/core/SkRect.h"
#include "include/core/SkRegion.h"
#include <array>
#include <functional>

namespace canvas::render::internal {
// Render-owner temporary resources only. Eight slots cover GPU/CPU full/edge
// tile extents (up to 2 MiB before backend COW/driver residency);
// snapshots keep Skia's copy-on-write semantics, never borrowed mutable pixels.
class CompactCoverageTargets final {
 public:
    std::shared_ptr<resources::ResourceBudgetManager> budget;
    sk_sp<SkSurface> acquire(SkCanvas&,int width,int height,bool compatible,CompactRasterWork*,bool scalar=false);
    void clear() noexcept {for(auto& slot:slots_)slot=Slot{};}
 private:
    struct Slot {sk_sp<SkSurface> surface;resources::ResourceLease lease;bool compatible=false,scalar=false;std::uint64_t used=0;};
    std::array<Slot,8> slots_{};
    std::uint64_t tick_=0;
};
// One device tile at a time, max-combined before material is applied once.
// No pointer to a FramePlan survives a yield. The owner validates source and
// target revisions before resuming with the current immutable knot span.
class CompactStrokeRasterJob final {
 public:
    CompactStrokeRasterJob(SkCanvas&,const foundation::InkBrushConfig&,
        std::span<const foundation::InkKnot>,const CompactDeviceGrid* = nullptr,bool compatible=false,
        std::shared_ptr<resources::ResourceBudgetManager> budget=nullptr,
        std::size_t maxRecordedBytes=RenderCacheOptions{}.maxEntryBytes,
        std::function<void(std::size_t)> reclaim={});
    bool step(SkCanvas&,const foundation::InkBrushConfig&,std::span<const foundation::InkKnot>,
        std::uint64_t seed,CompactCoverageTargets&,bool compatible,CompactRasterWork*);
 private:
    SkMatrix matrix_{};
    SkIRect clip_{},deviceClip_{};
    SkRect support_{};
    SkRegion interior_{},tileInterior_{};
    resources::ResourceLease interiorLease_;
    sk_sp<SkSurface> correction_;
    bool hybrid_=false;
    bool canCull_=false,done_=false;
    int x_=0,y_=0;
    std::size_t knot_=0;
    double distance_=0,totalDistance_=0;
    std::optional<ink::CompactContourCompiler> contour_;
    sk_sp<SkSurface> tile_;
    struct ContourRecord {
        sk_sp<SkPicture> picture;resources::ResourceLease lease;SkRect bounds{};
        std::optional<ink::CompactContourCompiler> checkpoint;
        bool empty=true;
    };
    struct RecordGroup {SkRect bounds{};std::size_t end=0;};
    static constexpr std::size_t recordsPerGroup=16,selectionChecksPerStep=32;
    std::vector<ContourRecord> contourRecords_;
    std::vector<RecordGroup> recordGroups_;
    std::shared_ptr<resources::ResourceBudgetManager> recordBudget_;
    resources::ResourceLease recordStorageLease_;
    SkRect recordBounds_{};
    std::size_t recordLimit_=0,recordBytes_=0,recordStorageBytes_=0;
    bool recordComplete_=false,picturesEnabled_=false;
    std::function<void(std::size_t)> reclaim_;
    void recordContour(const foundation::InkBrushConfig&,const ink::CompactContour&,ink::CompactContourCompiler);
    void appendRecord(ContourRecord);
    void abandonRecords() noexcept;
    void abandonPictures() noexcept;
};
}
