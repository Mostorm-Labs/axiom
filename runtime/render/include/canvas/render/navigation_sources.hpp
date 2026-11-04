#pragma once
#include "canvas/render/reference_draw_list.hpp"
#include <functional>
#include <type_traits>

namespace canvas::render {
// Internal render query. The Runtime owns Scene traversal/order; Render may
// request an expanded world region, never inspect or mutate the document.
struct NavigationSourceDiagnostics final {
    std::uint64_t chunkCandidates=0,indexNodes=0,indexFallbacks=0,payloadCopyBytes=0,payloadAliases=0;
    bool observed=false;
};
class NavigationSourceQuery final {
    std::function<bool(const foundation::WorldRect&,std::vector<ReferenceTraversalEntry>&,NavigationSourceDiagnostics&)> query_;
  public:
    NavigationSourceQuery()=default;
    NavigationSourceQuery(std::nullptr_t)noexcept{}
    template<class F>requires(!std::is_same_v<std::remove_cvref_t<F>,NavigationSourceQuery>)
    NavigationSourceQuery(F f){
        if constexpr(std::is_invocable_r_v<bool,F,const foundation::WorldRect&,std::vector<ReferenceTraversalEntry>&,NavigationSourceDiagnostics&>)
            query_=std::move(f);
        else query_=[f=std::move(f)](const auto& r,auto& out,NavigationSourceDiagnostics&)mutable{return f(r,out);};
    }
    explicit operator bool()const noexcept{return bool(query_);}
    bool operator()(const foundation::WorldRect& r,std::vector<ReferenceTraversalEntry>& out,NavigationSourceDiagnostics& diagnostics)const{return query_(r,out,diagnostics);}
    bool operator()(const foundation::WorldRect& r,std::vector<ReferenceTraversalEntry>& out)const{NavigationSourceDiagnostics ignored;return query_(r,out,ignored);}
};
// True only when the stamped Scene query proves no object overlaps this region.
// Uncertain/failed queries return false. Never inferred from render chunk hints.
using NavigationBackgroundQuery = std::function<bool(const foundation::WorldRect&)>;
struct NavigationWork final {
    bool pending=false, presented=false, coverageComplete=false;
    std::uint64_t tilesBuilt=0, knotSteps=0, sourceSteps=0, fallbackUses=0, evictions=0;
    std::size_t retainedBytes=0;
    std::uint64_t queries=0,initializations=0,cancelledJobs=0,discardedKnotSteps=0;
    std::uint64_t feedbackFirst=0,budgetYields=0,gpuThrottles=0,prewarmTiles=0,levelChanges=0;
    std::uint64_t queryNanoseconds=0,initializationNanoseconds=0,opportunityNanoseconds=0,overBudget=0;
    std::uint64_t prefixScanKnots=0;
    // Populated only in the opt-in diagnostic build.
    std::uint64_t overlayNanoseconds=0;
    std::uint64_t preparedGeometryBuilds=0,preparedGeometryHits=0,preparedGeometryDenied=0;
    std::uint64_t contourPrimitivesTested=0,contourPrimitivesDrawn=0,vectorPreparedUses=0;
    std::uint64_t gpuContourPrimitives=0,coarseMaskStarts=0;
    std::size_t preparedGeometryBytes=0;
    std::int32_t selectedLevel=0;
    // Gauges for the last presented navigation plane, not canonical quality.
    double maximumUpscale=0,lowResolutionFraction=0,retainedFraction=0;
    std::uint64_t suspendedRefinements=0,resumedRefinements=0;
    std::uint64_t chunkCandidates=0,indexNodes=0,indexFallbacks=0,payloadCopyBytes=0,payloadAliases=0,observedSourceQueries=0;
    std::uint64_t sourcePayloadHits=0;std::size_t sourcePayloadBytes=0;
};
}
