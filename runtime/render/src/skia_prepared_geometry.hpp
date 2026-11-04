#pragma once
#include "canvas/render/reference_draw_list.hpp"
#include "canvas/render/render_cache.hpp"
#include "canvas/resources/resource_budget_manager.hpp"
#include "canvas/ink/compact_contour.hpp"
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
    std::size_t primitiveCount()const noexcept{return ribbons_.size()+caps_.size();}
    PreparedGeometryDraw drawContour(SkCanvas&)const;
 private:
    friend class PreparedGeometryCache;
    struct Group {SkRect bounds;std::size_t begin=0,end=0;};
    ReferenceCommand source_;
    bool contour_=false;
    std::vector<double> distances_;
    std::optional<SkPath> vector_;
    std::vector<SkPath> ribbons_;
    std::vector<ink::ContourCap> caps_;
    std::vector<Group> ribbonGroups_,capGroups_;
    resources::ResourceLease lease_;
    std::shared_ptr<PreparedGeometryUsage> usage_;
    std::size_t bytes_=0;
    void build();
    std::size_t footprint()const;
};
class PreparedGeometryCache final {
 public:
    std::shared_ptr<const PreparedNavigationGeometry> get(const ReferenceTraversalEntry&,
        const RenderCacheOptions&,const std::shared_ptr<resources::ResourceBudgetManager>&);
    void clear()noexcept{entries_.clear();}
    PreparedGeometryStatistics statistics()const noexcept{auto s=work_;s.residentBytes=usage_->bytes;return s;}
 private:
    struct Entry {std::shared_ptr<const PreparedNavigationGeometry> geometry;std::uint64_t used=0;};
    std::unordered_map<foundation::ObjectId,Entry,foundation::ObjectIdHash> entries_;
    std::shared_ptr<PreparedGeometryUsage> usage_=std::make_shared<PreparedGeometryUsage>();
    std::weak_ptr<resources::ResourceBudgetManager> owner_;
    PreparedGeometryStatistics work_;
    std::uint64_t tick_=0;
    bool evictUnused();
};
}
