#pragma once
#include "canvas/render/reference_draw_list.hpp"
#include "canvas/render/render_cache.hpp"
#include "canvas/resources/resource_budget_manager.hpp"
#include "canvas/ink/compact_contour.hpp"
#include "source_retention.hpp"
#include "include/core/SkPath.h"
#include <unordered_map>

class SkCanvas;
namespace canvas::render::internal {
struct PreparedGeometryUsage final {std::size_t bytes=0;};
struct PreparedGeometryStatistics final {
    std::uint64_t builds=0,hits=0,denied=0,evictions=0;
    std::size_t residentBytes=0;
};
struct PreparedGeometryDraw final {std::uint64_t chunksVisited=0,primitivesTested=0,primitivesDrawn=0;};
// Render-private immutable world geometry. Source equality is checked in full:
// legacy Scene digest strings are not complete content identities for all ink.
class PreparedNavigationGeometry final {
 public:
    ~PreparedNavigationGeometry();
    const SkPath* vectorPath()const noexcept{return vector_?&*vector_:nullptr;}
    bool isContour()const noexcept{return contour_;}
    std::span<const double> distances()const noexcept{return distances_;}
    std::size_t primitiveCount()const noexcept{return replay_.empty()?ribbons_.size()+caps_.size():replay_.back().last;}
    double totalDistance()const noexcept{return distances_.empty()?replayDistance_:distances_.back();}
    std::size_t nextVisiblePrimitive(SkCanvas&,std::size_t first,const SkRect* deviceDemand=nullptr)const;
    PreparedGeometryDraw drawContour(SkCanvas&,std::size_t first=0,std::size_t count=~std::size_t{0},const SkRect* deviceDemand=nullptr)const;
 private:
    friend class PreparedGeometryCache;
    struct Group {SkRect bounds;std::size_t begin=0,end=0;};
    struct ReplayBlock {ink::CompactContourCompiler compiler;std::size_t knot=0,count=0,first=0,last=0;SkRect bounds;};
    static constexpr std::size_t replayBlockKnots=256,replayBlockExtras=32;
    std::vector<ReplayBlock> replay_;
    // Owner-thread draw scratch; source/checkpoint geometry stays immutable.
    // 16 taper cuts, carried patch and finish fit within32 extra slots.
    mutable ink::CompactContour replayOutput_;
    double replayDistance_=0;
    ReferenceCommand source_;
    bool contour_=false;
    std::vector<double> distances_;
    std::optional<SkPath> vector_;
    std::vector<SkPath> ribbons_;
    std::vector<ink::ContourCap> caps_;
    std::vector<Group> ribbonGroups_,capGroups_;
    SourceRetentions sourceLeases_;
    resources::ResourceLease lease_;
    std::shared_ptr<PreparedGeometryUsage> usage_;
    std::size_t bytes_=0;
    void build(ink::CompactContour* staged=nullptr);
    void buildReplay();
    std::size_t footprint()const;
};
class PreparedGeometryCache final {
 public:
    std::shared_ptr<const PreparedNavigationGeometry> get(const ReferenceTraversalEntry&,
        const RenderCacheOptions&,const std::shared_ptr<resources::ResourceBudgetManager>&,bool criticalReplay=false);
    void clear()noexcept{entries_.clear();}
    PreparedGeometryStatistics statistics()const noexcept{auto s=work_;s.residentBytes=usage_->bytes+retention_->statistics().bytes;return s;}
    void setRetentionPool(std::shared_ptr<SourceRetentionPool> pool){if(retention_!=pool){clear();retention_=std::move(pool);}}
    std::size_t preparedBytes()const noexcept{return usage_->bytes;}
    const SourceRetentionUsage& sourceStatistics()const noexcept{return retention_->statistics();}
    std::optional<SourceRetentions> retainSource(const ReferenceTraversalEntry& e,
        const std::shared_ptr<resources::ResourceBudgetManager>& budget,bool critical){return retention_->get(e,budget,critical);}
 private:
    struct Entry {std::shared_ptr<const PreparedNavigationGeometry> geometry;std::uint64_t used=0;};
    std::unordered_map<foundation::ObjectId,Entry,foundation::ObjectIdHash> entries_;
    std::shared_ptr<PreparedGeometryUsage> usage_=std::make_shared<PreparedGeometryUsage>();
    std::weak_ptr<resources::ResourceBudgetManager> owner_;
    std::shared_ptr<SourceRetentionPool> retention_=std::make_shared<SourceRetentionPool>();
    PreparedGeometryStatistics work_;
    std::uint64_t tick_=0;
    bool evictUnused();
};
}
