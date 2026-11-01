#pragma once
#include "canvas/render/reference_draw_list.hpp"
#include <functional>

namespace canvas::render {
// Internal render query. The Runtime owns Scene traversal/order; Render may
// request an expanded world region, never inspect or mutate the document.
using NavigationSourceQuery = std::function<bool(
    const foundation::WorldRect&, std::vector<ReferenceTraversalEntry>&)>;
struct NavigationWork final {
    bool pending=false, presented=false, coverageComplete=false;
    std::uint64_t tilesBuilt=0, knotSteps=0, sourceSteps=0, fallbackUses=0, evictions=0;
    std::size_t retainedBytes=0;
    std::uint64_t queries=0,initializations=0,cancelledJobs=0,discardedKnotSteps=0;
    std::uint64_t feedbackFirst=0,budgetYields=0,gpuThrottles=0,prewarmTiles=0,levelChanges=0;
    std::uint64_t queryNanoseconds=0,initializationNanoseconds=0,opportunityNanoseconds=0,overBudget=0;
    std::int32_t selectedLevel=0;
};
}
