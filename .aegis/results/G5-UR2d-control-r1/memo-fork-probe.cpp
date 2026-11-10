#include "canvas/render/skia_renderer.hpp"
#include "canvas/ink/brush_package_catalog.hpp"
#include "/Users/qing/.codex/worktrees/axiom-geometry-navigation/axiom/runtime/render/src/selection_region_geometry_internal.hpp"
#include "/Users/qing/.codex/worktrees/axiom-geometry-navigation/axiom/runtime/render/src/skia_prepared_geometry.hpp"
#include <iostream>
using namespace canvas;using namespace canvas::render;using namespace canvas::render::internal;
int main(){
 auto b=std::make_shared<resources::ResourceBudgetManager>();PreparedGeometryCache bodies;EraseMaskPathCache cache;RenderCacheOptions options;
 RuntimeSceneRecord r;r.objectId=ObjectId::fromUint64(1);r.kind=semantic::ObjectKind::kVectorStroke;r.kindVersion=3;semantic::CompactStrokeContent c;c.stroke.config=*ink::BrushPackageCatalog{}.loadDefault("round-handdrawn",4).package.compactConfig;c.stroke.config.size=20;c.stroke.config.thinning=0;c.stroke.knots={{100,100,32768}};r.content=c;
 semantic::FilledPathMask f;f.path.commands={semantic::MoveTo{{80,80}},semantic::LineTo{{120,80}},semantic::LineTo{{120,120}},semantic::LineTo{{80,120}},semantic::ClosePath{}};semantic::EraseMaskGeometry q=f;
 auto hit=cache.query(r,q,b,128*1024,&bodies,&options);auto before=cache.work().equivalentQueryHits;auto repeat=cache.query(r,q,b,128*1024,&bodies,&options);if(hit!=SelectionGeometryHit::kHit||repeat!=hit||cache.work().equivalentQueryHits!=before+1)return 1;
 auto changed=r;semantic::SweptCircleMask big;for(unsigned i=0;i<80;++i)big.segments.push_back({{{100,100},50},{{101,100},50},{100.3,100},{100.6,100}});changed.eraseMasks.push_back({ObjectId::fromUint64(2),big});before=cache.work().equivalentQueryHits;auto got=cache.query(changed,q,b,128*1024,&bodies,&options),oracle=intersectEraseGeometryComponentOracle(changed,q);if(got!=oracle||got!=SelectionGeometryHit::kMiss||cache.work().equivalentQueryHits!=before)return 2;
 for(unsigned i=0;i<24;++i){auto test=changed;if(i%3==0)test.eraseMasks.clear();if(i%4==0)test.content.update([](auto& x){std::get<semantic::CompactStrokeContent>(x).stroke.knots[0].x=105;});test.transform.tx=i*.125;auto expected=intersectEraseGeometryComponentOracle(test,q),actual=cache.query(test,q,b,128*1024,&bodies,&options);if(expected!=actual)return 3;}
 cache.clear();bodies.clear();std::cout<<"memo_hit_proven=1 denied_history_safe=1 fork_queries=24 leases="<<b->used()<<std::endl;return b->used()?4:0;
}
