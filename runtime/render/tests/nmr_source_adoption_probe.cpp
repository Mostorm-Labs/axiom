#include "selection_region_geometry_internal.hpp"
#include "navigation_mask_plan.hpp"
#include <iostream>
using namespace canvas;
using namespace canvas::render::internal;
int main(){
 auto budget=std::make_shared<resources::ResourceBudgetManager>();auto pool=std::make_shared<SourceRetentionPool>();EraseMaskPathCache cache;cache.setRetentionPool(pool);
 semantic::SweptCircleMask s;for(unsigned i=0;i<128;++i)s.segments.push_back({{{10,10},2},{{30,30},2},{15,12},{25,28}});
 scene::SharedVector<semantic::EraseMaskRecord> h{{foundation::ObjectId::fromUint64(1),std::move(s)}};
 auto p=cache.requestNavigationHistory(h,budget,8U*1048576U);if(!p)return 2;
 if(cache.advanceNavigationHistory(p)!=EraseMaskPathCache::NavigationMaskStatus::kPending)return 3;
 auto canonical=pool->getMasks(h,budget,true);if(!canonical)return 4;
 for(unsigned turns=0;!p->ready;++turns)if(turns>=10||cache.advanceNavigationHistory(p)==EraseMaskPathCache::NavigationMaskStatus::kInvalid)return 5;
 const auto retained=p->accounting.size();const auto bytes=budget->used();p->accounting.clear();const auto excess=bytes-budget->used();
 std::cout<<"{\"case\":\"canonical-publishes-source-while-navigation-preparing\",\"same_source\":"<<(p->source==canonical?"true":"false")<<",\"redundant_staging_leases\":"<<retained<<",\"duplicate_charged_bytes\":"<<excess<<",\"canonical_source_bytes\":"<<canonical->bytes<<"}"<<std::endl;
 return retained==0&&excess==0&&p->source==canonical?0:6;
}
