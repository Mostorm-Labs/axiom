#pragma once
#include "canvas/render/selection_region_geometry.hpp"
#include "canvas/resources/resource_budget_manager.hpp"
#include "canvas/render/render_cache.hpp"
#include "canvas/render/reference_draw_list.hpp"
#include <functional>
class SkPath;
namespace canvas::render::internal {
class PreparedGeometryCache;
class SourceRetentionPool;
struct MaskHistoryPlan;
struct ExactHistoryProof;
class PreparedNavigationGeometry;
struct PreparationAllowance;
enum class MaskHistoryAdmission { kReady, kNotAdmitted, kResourcePending };
// Independent component-order oracle retained for fallback qualification.
// Private source header; no consumer bridge/package export.
SelectionGeometryHit intersectEraseGeometryComponentOracle(const RuntimeSceneRecord&,
    const semantic::EraseMaskGeometry&) noexcept;
struct ExactQueryWork final {
    std::uint64_t queries=0,fullValidations=0,validationKnots=0,invalidValidationScans=0;
    std::uint64_t compileCalls=0,compileInputs=0,constructedComponents=0;
    std::uint64_t bodyUses=0,indexNodes=0,componentChecks=0,replayKnots=0,fallbacks=0;
    std::uint64_t preparedValidationKnots=0,preparedCompileCalls=0,preparedCompileInputs=0,preparedComponents=0,contentComparisonKnots=0;
    std::uint64_t materializedPaths=0,boundsTests=0,bodyIdentity=0,fallbackReason=0;
};
class EraseMaskPathCache final {
 public:
    struct Entry;
    static bool readyMatches(const std::shared_ptr<const Entry>&,const semantic::EraseMaskGeometry&)noexcept;
    static const SkPath& readyPath(const std::shared_ptr<const Entry>&)noexcept;
    void setResidentUpperBound(std::size_t)noexcept;
    std::shared_ptr<const Entry> lookup(const semantic::EraseMaskGeometry&)noexcept;
    std::shared_ptr<const Entry> publishFinished(const semantic::EraseMaskGeometry&,const SkPath&,
        std::shared_ptr<resources::ResourceBudgetManager>,std::size_t);
    struct Work {std::uint64_t builds=0,hits=0,denied=0,pathValueComparisons=0,pathPrimitiveComparisons=0;std::size_t bytes=0;ExactQueryWork exact;MaskHistoryWork history;std::uint64_t localRefusals=0,refusalHits=0,resourceWaits=0,equivalentQueryHits=0;};
    EraseMaskPathCache();
    ~EraseMaskPathCache();
    SelectionGeometryHit query(const RuntimeSceneRecord&,const semantic::EraseMaskGeometry&,
        std::shared_ptr<resources::ResourceBudgetManager>,std::size_t capacity,
        PreparedGeometryCache* prepared=nullptr,const RenderCacheOptions* options=nullptr,const SkPath* provenQuery=nullptr,bool strictReady=false)noexcept;
    SelectionGeometryHit selectionQuery(const RuntimeSceneRecord&,std::span<const foundation::WorldPoint>,
        std::shared_ptr<resources::ResourceBudgetManager>,std::size_t capacity,
        PreparedGeometryCache*,const RenderCacheOptions*)noexcept;
    void setRetentionPool(std::shared_ptr<SourceRetentionPool>);
    std::shared_ptr<MaskHistoryPlan> history(const scene::SharedVector<semantic::EraseMaskRecord>&,
        std::shared_ptr<resources::ResourceBudgetManager>,std::size_t capacity,
        const std::shared_ptr<MaskHistoryPlan>& previous={},MaskHistoryAdmission* admission=nullptr);
    std::shared_ptr<const Entry> historicalPath(const std::shared_ptr<MaskHistoryPlan>&,std::size_t index);
    std::shared_ptr<const Entry> exactHistoricalPath(const std::shared_ptr<ExactHistoryProof>&,std::size_t index);
    // Private owner-thread scope: bounded current query, no document ownership.
    void beginCohort(std::function<void()> pressure={}) noexcept;
    void endCohort() noexcept;
    // Private Render translation units only; returned pins retain their quota.
    std::shared_ptr<const Entry> get(const semantic::EraseMaskGeometry&,bool historical=false);
    // Optional sequential PathOps prefix advanced by the existing live frame.
    // A final query still verifies the entire input and finishes any remainder.
    void prepareQueryPrefix(const semantic::EraseMaskGeometry&,
        std::shared_ptr<resources::ResourceBudgetManager>,std::size_t capacity,
        std::size_t maxSegments=8)noexcept;
    void discardQueryPrefixes()noexcept;
    void prepareQueryPrefixes(std::span<const ReferenceTraversalEntry>,
        std::shared_ptr<resources::ResourceBudgetManager>,std::size_t capacity,PreparationAllowance&)noexcept;
    void clear()noexcept;
    Work work()const noexcept;
 private:
    void advanceQueryPrefix(const semantic::EraseMaskGeometry&,
        std::shared_ptr<resources::ResourceBudgetManager>,std::size_t capacity,
        std::size_t maxSegments,PreparationAllowance*)noexcept;
    struct Impl;
    std::shared_ptr<ExactHistoryProof> exactHistory(const scene::SharedVector<semantic::EraseMaskRecord>&);
    std::unique_ptr<Impl> impl_;
};
}
