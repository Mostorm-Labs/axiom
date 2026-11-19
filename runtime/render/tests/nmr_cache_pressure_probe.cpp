#include "selection_region_geometry_internal.hpp"
#include "navigation_mask_plan.hpp"
#include <algorithm>
#include <iostream>
#include <vector>
using namespace canvas;
using namespace canvas::render::internal;
scene::SharedVector<semantic::EraseMaskRecord> history(unsigned id){
 semantic::SweptCircleMask s;s.segments.push_back({{{10,10},2},{{30,30},2},{15,12},{25,28}});
 return {{foundation::ObjectId::fromUint64(id),std::move(s)}};
}
int main(){
 auto budget=std::make_shared<resources::ResourceBudgetManager>(resources::BudgetLimits{512U*1048576U,1024U*1048576U});
 EraseMaskPathCache cache;std::vector<std::shared_ptr<NavigationMaskPlan>> plans;
 for(unsigned i=1;i<=512;++i){auto p=cache.requestNavigationHistory(history(i),budget,8U*1048576U);if(!p)return 2;plans.push_back(std::move(p));}
 NavigationMaskIdentityLess less;
 std::sort(plans.begin(),plans.end(),[&](const auto&a,const auto&b){return less({{a->records.storage(),sourceTypeTag<std::vector<semantic::EraseMaskRecord>>(),0},a->budget},{{b->records.storage(),sourceTypeTag<std::vector<semantic::EraseMaskRecord>>(),0},b->budget});});
 const auto used=cache.work().bytes;
 plans.resize(64); // First64identities pinned; remaining448ownedonlybycache.
 auto h=history(700);unsigned denied=0;
 for(unsigned i=0;i<256;++i){auto p=cache.requestNavigationHistory(h,budget,8U*1048576U);if(!p)++denied;else break;}
 plans.clear();auto after=cache.requestNavigationHistory(h,budget,8U*1048576U);
 std::cout<<"{\"case\":\"pinned-prefix-idle-suffix-request-progress\",\"cache_rows\":512,\"pinned_prefix\":64,\"idle_suffix\":448,\"denied_retries\":"<<denied<<",\"bytes_before\":"<<used<<",\"succeeds_after_unpin\":"<<(after?"true":"false")<<"}"<<std::endl;
 return denied<256&&bool(after)?0:3;
}
