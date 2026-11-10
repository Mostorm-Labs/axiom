#include "canvas/render/skia_renderer.hpp"
#include "canvas/ink/brush_package_catalog.hpp"
#include "/Users/qing/.codex/worktrees/axiom-geometry-navigation/axiom/runtime/render/src/selection_region_geometry_internal.hpp"
#include "/Users/qing/.codex/worktrees/axiom-geometry-navigation/axiom/runtime/render/src/skia_prepared_geometry.hpp"
#include <nlohmann/json.hpp>
#include <iostream>
#include <chrono>
#include <cmath>
using namespace canvas;using namespace canvas::render;using namespace canvas::render::internal;
semantic::EraseMaskGeometry rectangle(double x,double y,double width,double height){semantic::FilledPathMask m;m.path.commands={semantic::MoveTo{{x,y}},semantic::LineTo{{x+width,y}},semantic::LineTo{{x+width,y+height}},semantic::LineTo{{x,y+height}},semantic::ClosePath{}};return m;}
int main(){try{RuntimeSceneRecord original;original.objectId=ObjectId::fromUint64(1);original.kind=semantic::ObjectKind::kVectorStroke;original.kindVersion=3;semantic::CompactStrokeContent c;c.stroke.config=*ink::BrushPackageCatalog{}.loadDefault("round-handdrawn",4).package.compactConfig;c.stroke.config.size=20;c.stroke.config.thinning=0;c.stroke.seed=42;c.stroke.knots={{100,100,32768}};original.content=c;auto query=rectangle(85,85,30,30);
 auto budget=std::make_shared<resources::ResourceBudgetManager>(resources::BudgetLimits{256U*1024U*1024U,512U*1024U*1024U});PreparedGeometryCache bodies;EraseMaskPathCache cache;RenderCacheOptions o;o.maxBytes=256U*1024U*1024U;o.sharedMaxBytes=512U*1024U*1024U;o.maxEntryBytes=64U*1024U*1024U;
 nlohmann::json rows=nlohmann::json::array();
 for(unsigned test=0;test<72;++test){auto r=original;const unsigned n=test%12;
  if(n<9){r.eraseMasks.push_back({ObjectId::fromUint64(2000+test*3),rectangle(80,80,20,40)});r.eraseMasks.push_back({ObjectId::fromUint64(2001+test*3),rectangle(100,80,20,40)});}
  if(n==1)std::swap(r.eraseMasks[0],r.eraseMasks[1]);if(n==2)r.eraseMasks.pop_back();
  if(n==3)r.eraseMasks.push_back({ObjectId::fromUint64(3000+test),rectangle(300,300,30,30)});
  if(n==4)std::get<semantic::LineTo>(std::get<semantic::FilledPathMask>(r.eraseMasks[1].geometry).path.commands[1]).end.x+=.015;
  if(n==5)r.content.update([](auto&v){std::get<semantic::CompactStrokeContent>(v).stroke.knots.front().x+=5;});
  if(n==6){auto q=std::get<semantic::CompactStrokeContent>(r.content.get());q.stroke.config.family=foundation::InkFamily::kMarker;q.stroke.config.markerAngle=.3;q.stroke.config.markerFlatness=.4;r.content=q;}
  if(n==7)r.content.update([](auto&v){std::get<semantic::CompactStrokeContent>(v).stroke.config.opacity=0;});
  if(n==8)r.content.update([](auto&v){std::get<semantic::CompactStrokeContent>(v).stroke.knots.clear();});
  if(n==9)r.eraseMasks.push_back({ObjectId::fromUint64(test+5000),rectangle(97,97,.03,6)});
  if(n==10)r.kindVersion+=1;
  if(test%3==0)r.transform={.8,.2,-.1,1.1,20,-15};
  auto q=query;if(test%4==0)q=rectangle(109.99,99.99,1,.02);if(test%5==0)q=rectangle(150,150,2,2);
  const auto before=cache.work().equivalentQueryHits;auto hit=cache.query(r,q,budget,8U*1024U*1024U,&bodies,&o);auto repeated=cache.query(r,q,budget,8U*1024U*1024U,&bodies,&o);if(hit!=repeated)return 1;rows.push_back({{"case",test},{"decision",unsigned(hit)},{"repeat_memo_hits",cache.work().equivalentQueryHits-before}});
 }
 cache.clear();bodies.clear();if(budget->used())return 2;std::cout<<nlohmann::json{{"rows",rows},{"clear_bytes",budget->used()}}.dump()<<std::endl;return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<std::endl;return 3;}}
