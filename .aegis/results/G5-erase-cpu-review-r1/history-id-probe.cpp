#include "canvas/render/skia_renderer.hpp"
#include "canvas/ink/brush_package_catalog.hpp"
#include "/Users/qing/.codex/worktrees/axiom-geometry-navigation/axiom/runtime/render/src/selection_region_geometry_internal.hpp"
#include "/Users/qing/.codex/worktrees/axiom-geometry-navigation/axiom/runtime/render/src/skia_prepared_geometry.hpp"
#include <iostream>
#include <cmath>
using namespace canvas;using namespace canvas::render;using namespace canvas::render::internal;
#include <chrono>
#include <nlohmann/json.hpp>
int main(){try{
 for(bool uniqueIds:{false,true})for(unsigned querySegments:{80U,400U}){
  auto budget=std::make_shared<resources::ResourceBudgetManager>(resources::BudgetLimits{256U*1024U*1024U,512U*1024U*1024U});PreparedGeometryCache bodies;EraseMaskPathCache cache;RenderCacheOptions opts;opts.maxBytes=256U*1024U*1024U;opts.sharedMaxBytes=512U*1024U*1024U;opts.maxEntryBytes=64U*1024U*1024U;
  RuntimeSceneRecord r;r.objectId=ObjectId::fromUint64(1);r.kind=semantic::ObjectKind::kVectorStroke;r.kindVersion=3;semantic::CompactStrokeContent c;c.stroke.config=*ink::BrushPackageCatalog{}.loadDefault("round-handdrawn",4).package.compactConfig;c.stroke.config.size=16;c.stroke.seed=42;
  for(unsigned i=0;i<7199;++i)c.stroke.knots.push_back({250+180*std::sin(i*.037),220+150*std::cos(i*.053),std::uint16_t(20000+(i%100)*300)});r.content=c;
  for(unsigned h=0;h<2;++h){semantic::SweptCircleMask mask;for(unsigned i=1;i<=80;++i){semantic::Vec2 a{240+70*std::sin((i-1)*.17+h*.1),210+65*std::cos((i-1)*.11)},z{240+70*std::sin(i*.17+h*.1),210+65*std::cos(i*.11)};mask.segments.push_back({{a,15},{z,15},{a.x+(z.x-a.x)/3,a.y+(z.y-a.y)/3},{a.x+2*(z.x-a.x)/3,a.y+2*(z.y-a.y)/3}});}r.eraseMasks.push_back({ObjectId::fromUint64(9000+h),mask});}
  semantic::SweptCircleMask q;for(unsigned i=1;i<=querySegments;++i){semantic::Vec2 a{410+9*std::sin((i-1)*.31),180+80.*(i-1)/querySegments},z{410+9*std::sin(i*.31),180+80.*i/querySegments};q.segments.push_back({{a,10},{z,10},{a.x+(z.x-a.x)/3,a.y+(z.y-a.y)/3},{a.x+2*(z.x-a.x)/3,a.y+2*(z.y-a.y)/3}});}
  auto start=std::chrono::steady_clock::now();unsigned hits=0;for(unsigned i=0;i<16;++i){auto alias=r;alias.objectId=ObjectId::fromUint64(100+i);if(uniqueIds)for(unsigned j=0;j<alias.eraseMasks.size();++j)alias.eraseMasks[j].id=ObjectId::fromUint64(20000+i*10+j);auto v=cache.query(alias,q,budget,8U*1024U*1024U,&bodies,&opts);if(v==SelectionGeometryHit::kHit)++hits;else if(v!=SelectionGeometryHit::kMiss)return 1;}
  const auto elapsed=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();auto w=cache.work();std::cout<<nlohmann::json{{"unique_canonical_history_ids",uniqueIds},{"copies",16},{"knots",7199},{"history_masks",2},{"history_segments",80},{"query_segments",querySegments},{"ms",elapsed},{"hits",hits},{"equivalent_query_hits",w.equivalentQueryHits},{"history_admissions",w.history.admissions},{"history_primitive_builds",w.history.primitiveBuilds},{"query_path_builds",w.history.queryPathBuilds},{"historical_path_builds",w.history.historyPathBuilds},{"body_builds",bodies.statistics().builds}}.dump()<<std::endl;cache.clear();bodies.clear();if(budget->used())return 2;
 }
 return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<std::endl;return 3;}}
