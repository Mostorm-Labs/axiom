#include "canvas/render/tile/tile_grid.hpp"
#include "canvas/scheduling/runtime_scheduler.hpp"
#include "canvas/resources/resource_budget_manager.hpp"
#include <iostream>
#include <stdexcept>
using namespace canvas;
void require(bool ok,const char* s){if(!ok)throw std::runtime_error(s);}
int main(){
 render::TileGrid tiles(256,1024);
 auto keys=tiles.coverage({-256,-1,256,257},0,20);
 require(keys.size()==6&&keys.front().x==-1&&keys.front().y==-1,"negative world tile coverage");
 tiles.markDirty({-100000,-100000,100000,100000});
 require(tiles.requests({0,0,256,256},0,1).empty(),"disabled tile candidate generated raster work");
 tiles.setEnabled(true);
 auto request=tiles.requests({0,0,256,256},0,1);
 require(request.size()==1,"offscreen dirty tiles immediately scheduled");
 const auto local=request.front().generation;
 require(tiles.complete(request.front()),"fresh tile rejected");
 tiles.markDirty({1024,1024,1280,1280});
 require(tiles.requests({0,0,256,256},0,1).empty(),"unrelated world damage invalidated clean tile");
 tiles.markDirty({0,0,10,10});
 require(!tiles.complete(request.front()),"stale tile result published");
 request=tiles.requests({0,0,256,256},0,1);require(request.size()==1&&request[0].generation>local,"local generation unchanged");
 scheduling::RuntimeScheduler queue(3);
 require(queue.submit({1,1,scheduling::WorkPriority::kBackground,0,{},1}),"dependency");
 require(queue.submit({2,1,scheduling::WorkPriority::kVisible,0,{1},1}),"visible");
 require(queue.submit({3,1,scheduling::WorkPriority::kPrefetch,0,{},1}),"prefetch");
 auto work=queue.take();require(work&&work->id==1,"visible dependency lacks inherited priority");
 require(queue.finish(1,1),"dependency completion");work=queue.take();require(work&&work->id==2,"visible work starved");
 require(queue.submit({3,2,scheduling::WorkPriority::kNear,0,{},1}),"supersede");
 require(!queue.finish(3,1),"stale work completion accepted");
 require(queue.cancel(3,2),"cancel");
 scheduling::RuntimeScheduler bounded(1);
 require(bounded.submit({10,1,scheduling::WorkPriority::kPrefetch,0,{},0}),"prefetch admit");
 require(bounded.submit({11,1,scheduling::WorkPriority::kVisible,0,{},0})&&bounded.size()==1,"visible cannot evict prefetch");
 scheduling::RuntimeScheduler cyclic(4);
 require(cyclic.submit({1,1,scheduling::WorkPriority::kBackground,0,{2},0}),"future dependency");
 require(!cyclic.submit({2,1,scheduling::WorkPriority::kVisible,0,{1},0}),"dependency cycle accepted");
 render::TileGrid reused(256,1);reused.setEnabled(true);
 auto old=reused.requests({0,0,256,256},0,1).front();reused.evict(old.key);
 auto fresh=reused.requests({0,0,256,256},0,1).front();
 require(fresh.generation!=old.generation&&!reused.complete(old),"evicted tile accepts old result");
 scheduling::RuntimeScheduler deadlines(2);
 require(deadlines.submit({1,1,scheduling::WorkPriority::kVisible,4,{},0}),"deadline admission");
 require(!deadlines.take(5),"expired work dispatched");
 scheduling::RuntimeScheduler generations(4);
 require(generations.submit({1,1,scheduling::WorkPriority::kBackground,0,{},0}),"dependency gen1");
 require(generations.take().has_value()&&generations.finish(1,1),"dependency gen1 complete");
 require(generations.submit({2,1,scheduling::WorkPriority::kVisible,0,{{1,2}},0}),"requires gen2");
 require(!generations.take(),"old dependency generation satisfies new work");
 require(generations.submit({1,2,scheduling::WorkPriority::kBackground,0,{},0}),"dependency gen2");
 require(generations.take()->id==1&&generations.finish(1,2),"dependency gen2 complete");
 require(generations.take()->id==2,"new dependency completion did not unblock work");
 resources::ResourceBudgetManager budget({100,120});
 auto a=budget.reserve(resources::ResourceClass::kRaster,80,true);
 require(bool(a),"visible reservation");
 require(!budget.reserve(resources::ResourceClass::kPromotion,30,false),"soft limit admitted promotion");
 require(!budget.reserve(resources::ResourceClass::kScratch,50,true),"hard limit exceeded");
 budget.release(*a);require(budget.used()==0,"reservation leaked");
 std::cout<<"tile local validity, dirty-lazy work, scheduler dependency and budget guards\n";
}
