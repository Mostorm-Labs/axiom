#pragma once
#include "canvas/render/reference_draw_list.hpp"
#include "canvas/render/render_cache.hpp"
#include "canvas/resources/resource_budget_manager.hpp"
#include "canvas/ink/compact_contour.hpp"
#include "source_retention.hpp"
#include "include/core/SkPath.h"
#include <map>
#include <array>
#include <chrono>
#include <functional>

class SkCanvas;
namespace canvas::render::internal {
struct PreparedGeometryUsage final {std::size_t bytes=0,indexBytes=0;};
enum class PreparedConsumer {kOther,kNavigation,kRetainedInk,kCanonical,kDirect,kExact};
class PreparedBodyJob;
ImmutableSourceIdentity preparedBodyIdentity(const ReferenceCommand&);
// One owner invocation allowance, shared with canonical raster preparation.
struct PreparationAllowance final {
    RenderPreparationBudget limits;
    std::chrono::steady_clock::time_point start=std::chrono::steady_clock::now();
    std::size_t steps=0;
    explicit PreparationAllowance(RenderPreparationBudget value={}):limits(value){}
    bool exhausted()const noexcept {return steps>=limits.maxCoverageSteps||!limits.maxMicroseconds||
        (steps&&std::chrono::steady_clock::now()-start>=std::chrono::microseconds(limits.maxMicroseconds));}
};
struct PreparedGeometryStatistics final {
    std::uint64_t builds=0,hits=0,denied=0,evictions=0;
    std::uint64_t lookups=0,indexLookups=0,identityHits=0,fingerprintBytes=0,contentComparisons=0,candidateChecks=0;
    std::uint64_t aliasAdmissions=0,aliasEvictions=0,navigationQueries=0,retainedInkQueries=0;
    std::size_t residentBytes=0,indexReservationBytes=0,aliases=0,bodies=0;
    std::uint64_t sourceFootprintEvaluations=0,sourceKnownFootprintHits=0;
    std::uint64_t exactQueries=0,contentComparisonKnots=0,validationKnots=0;
    std::uint64_t syncCompileCalls=0,syncCompileInputs=0,syncPathPrimitives=0,syncLengthInputs=0;
    std::uint64_t canonicalQueries=0,directQueries=0,jobAdmissions=0,advances=0;
    std::uint64_t supportKnots=0,distanceInputs=0,compileInputs=0,pathPrimitives=0,maxAdvanceKnots=0;
};
struct ExactComponentWork final {
    std::uint64_t groups=0,boundsTests=0,components=0,replayKnots=0,materializedPaths=0;
};
struct PreparedGeometryDraw final {std::uint64_t chunksVisited=0,primitivesTested=0,primitivesDrawn=0,compiledKnots=0;};
// Render-private immutable world geometry. Source equality is checked in full:
// legacy Scene digest strings are not complete content identities for all ink.
class PreparedNavigationGeometry final {
 public:
    ~PreparedNavigationGeometry();
    bool failed()const noexcept{return failed_;}
    bool ready()const noexcept{return ready_;}
    const SkRect& support()const noexcept{return support_;}
    const SkPath* vectorPath()const noexcept{return vector_?&*vector_:nullptr;}
    bool isContour()const noexcept{return contour_;}
    bool exactValidated()const noexcept{return ready_&&exactValidationComplete_&&exactValidationValid_&&exactDerivedFinite_;}
    std::uint64_t identity()const noexcept{return cacheId_;}
    // Exact CPU components only: bounds are negative filters, never a hit.
    // Visitor false stops at a decided component; source/checkpoints stay pinned.
    bool visitExact(const SkRect&,const std::function<bool(const SkPath&)>&,ExactComponentWork&)const;
    std::span<const double> distances()const noexcept{return distances_;}
    std::size_t primitiveCount()const noexcept{return replay_.empty()?ribbons_.size()+caps_.size():replay_.back().last;}
    double totalDistance()const noexcept{return distances_.empty()?replayDistance_:distances_.back();}
    std::size_t nextVisiblePrimitive(SkCanvas&,std::size_t first,const SkRect* deviceDemand=nullptr)const;
    PreparedGeometryDraw drawContour(SkCanvas&,std::size_t first=0,std::size_t count=~std::size_t{0},const SkRect* deviceDemand=nullptr)const;
 private:
    friend class PreparedGeometryCache;
    friend class PreparedBodyJob;
    bool ready_=true,failed_=false;
    bool exactValidationComplete_=false,exactValidationValid_=false;
    // Complete actual SkPath finiteness, not a semantic coordinate restriction.
    bool exactDerivedFinite_=true;
    static SkPath exactCapPath(const foundation::InkBrushConfig&,const ink::ContourCap&);
    std::uint64_t cacheId_=0;
    SkRect support_=SkRect::MakeEmpty();
    std::unique_ptr<PreparedBodyJob> job_;
    struct Group {SkRect bounds;std::size_t begin=0,end=0;};
    struct ReplayBlock {ink::CompactContourCompiler compiler;std::size_t knot=0,count=0,first=0,last=0;SkRect bounds;};
    static constexpr std::size_t replayBlockKnots=256,replayBlockExtras=32;
    std::vector<ReplayBlock> replay_;
    // Owner-thread draw scratch; source/checkpoint geometry stays immutable.
    // 16 taper cuts, carried patch and finish fit within32 extra slots.
    mutable ink::CompactContour replayOutput_;
    // The existing charged scratch may retain one immutable checkpoint output
    // between adjacent owner-thread draw ranges. Exact queries also use this
    // scratch and invalidate the draw witness before writing it.
    mutable const ReplayBlock* replayOutputBlock_=nullptr;
    double replayDistance_=0;
    ReferenceCommand source_;
    bool contour_=false;
    std::vector<double> distances_;
    std::optional<SkPath> vector_;
    std::vector<SkPath> ribbons_;
    std::vector<ink::ContourCap> caps_;
    std::vector<Group> ribbonGroups_,capGroups_;
    SourceRetentions sourceLeases_;
    // Fixed initial index reservations stay with an in-flight body. Later
    // aliases remain independently evictable and never accumulate here.
    std::array<std::shared_ptr<const void>,2> initialIndexLeases_;
    resources::ResourceLease lease_;
    std::shared_ptr<PreparedGeometryUsage> usage_;
    std::size_t bytes_=0;
    void build(ink::CompactContour* staged=nullptr,PreparedGeometryStatistics* work=nullptr);
    void buildReplay(PreparedGeometryStatistics* work=nullptr);
    std::size_t footprint()const;
};
class PreparedGeometryCache final {
 public:
    std::shared_ptr<const PreparedNavigationGeometry> get(const ReferenceTraversalEntry&,
        const RenderCacheOptions&,const std::shared_ptr<resources::ResourceBudgetManager>&,bool criticalReplay=false,PreparedConsumer consumer=PreparedConsumer::kOther,bool deferred=false,bool existingOnly=false);
    // Lookup never starts/drains preparation. Request can publish an incomplete
    // private body; only advance may make that body ready.
    std::shared_ptr<const PreparedNavigationGeometry> lookup(const ReferenceTraversalEntry&,PreparedConsumer);
    std::shared_ptr<const PreparedNavigationGeometry> request(const ReferenceTraversalEntry& e,const RenderCacheOptions& o,
        const std::shared_ptr<resources::ResourceBudgetManager>& b){lastRequestRejected_=false;return get(e,o,b,true,PreparedConsumer::kOther,true);}
    bool requestRejected()const noexcept{return lastRequestRejected_;}
    bool advance(const std::shared_ptr<const PreparedNavigationGeometry>&,PreparationAllowance&);
    void clear()noexcept;
    bool reclaimUnused(){return evictUnused();}
    PreparedGeometryStatistics statistics()const noexcept{auto s=work_;const auto& source=retention_->statistics();
        s.indexReservationBytes=usage_->indexBytes+source.indexBytes;s.residentBytes=usage_->bytes+source.bytes+s.indexReservationBytes;
        s.aliases=aliases_.size();s.bodies=entries_.size();s.sourceFootprintEvaluations=source.footprintEvaluations;
        s.sourceKnownFootprintHits=source.knownFootprintHits;return s;}
    void setRetentionPool(std::shared_ptr<SourceRetentionPool> pool){if(pool&&retention_!=pool){clear();retention_=std::move(pool);}}
    std::size_t preparedBytes()const noexcept{return usage_->bytes;}
    const SourceRetentionUsage& sourceStatistics()const noexcept{return retention_->statistics();}
    std::optional<SourceRetentions> retainSource(const ReferenceTraversalEntry& e,
        const std::shared_ptr<resources::ResourceBudgetManager>& budget,bool critical);
 private:
    struct IndexLease {
        resources::ResourceLease lease;std::shared_ptr<PreparedGeometryUsage> usage;std::size_t bytes=0;
        ~IndexLease(){if(usage)usage->indexBytes-=bytes;}
    };
    struct Entry {std::shared_ptr<const PreparedNavigationGeometry> geometry;std::uint64_t used=0,fingerprint=0;std::shared_ptr<IndexLease> metadata;};
    struct Alias {std::weak_ptr<const PreparedNavigationGeometry> geometry;std::uint64_t body=0,used=0;SourceRetentions sources;std::shared_ptr<IndexLease> metadata;};
    std::map<std::uint64_t,Entry> entries_;
    std::multimap<std::uint64_t,std::uint64_t> fingerprints_;
    std::map<ImmutableSourceIdentity,Alias,ImmutableSourceIdentityLess> aliases_;
    std::shared_ptr<PreparedGeometryUsage> usage_=std::make_shared<PreparedGeometryUsage>();
    std::weak_ptr<resources::ResourceBudgetManager> owner_;
    std::shared_ptr<SourceRetentionPool> retention_=std::make_shared<SourceRetentionPool>();
    PreparedGeometryStatistics work_;
    bool lastRequestRejected_=false;
    std::uint64_t tick_=0,serial_=0;
    std::shared_ptr<IndexLease> reserveIndex(std::size_t,const std::shared_ptr<resources::ResourceBudgetManager>&,bool);
    bool remember(const ImmutableSourceIdentity&,std::shared_ptr<const PreparedNavigationGeometry>,std::uint64_t,
        SourceRetentions,const RenderCacheOptions&,const std::shared_ptr<resources::ResourceBudgetManager>&,bool);
    std::shared_ptr<const PreparedNavigationGeometry> publish(std::shared_ptr<PreparedNavigationGeometry>,std::uint64_t,
        const ImmutableSourceIdentity&,SourceRetentions,const RenderCacheOptions&,const std::shared_ptr<resources::ResourceBudgetManager>&,bool);
    void eraseBody(std::uint64_t)noexcept;
    bool evictAlias()noexcept;
    void bindOwner(const std::shared_ptr<resources::ResourceBudgetManager>&);
    bool evictUnused();
};
#if defined(AXIOM_ENABLE_FRAME_TIMING)
// One bounded aggregate per existing renderer invocation, not per point/get.
class PreparedBodyTrace final {
    std::shared_ptr<const PreparedGeometryCache> cache_;
    PreparedGeometryStatistics before_;
    const char* purpose_;
 public:
    PreparedBodyTrace(std::shared_ptr<const PreparedGeometryCache> cache,const char* purpose)
        :cache_(std::move(cache)),before_(cache_?cache_->statistics():PreparedGeometryStatistics{}),purpose_(purpose){}
    ~PreparedBodyTrace();
};
#define AXIOM_PREPARED_TRACE(name,cache,purpose) ::canvas::render::internal::PreparedBodyTrace name{cache,purpose}
#else
#define AXIOM_PREPARED_TRACE(name,cache,purpose) ((void)0)
#endif

}
