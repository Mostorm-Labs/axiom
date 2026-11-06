#pragma once
#include "canvas/resources/resource_budget_manager.hpp"

namespace canvas::render::internal {
// Eviction planning only. Queued GPU bytes stay charged in reserve() until the
// actual completion callback; this projection cannot grant an allocation.
inline std::size_t chargeAfterRetirement(const resources::ResourceBudgetManager& budget)noexcept {
    auto charged=budget.used();
    for(unsigned i=0;i<static_cast<unsigned>(resources::ResourceClass::kCount);++i)
        charged-=std::min(charged,budget.snapshot(static_cast<resources::ResourceClass>(i)).pendingRelease);
    return charged;
}
inline bool releasedQuotaSatisfies(const resources::ResourceBudgetManager& budget,
    std::size_t limit,std::size_t requested)noexcept {
    return requested<=limit&&chargeAfterRetirement(budget)<=limit-requested;
}
}
