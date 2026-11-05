#include "canvas/render/tile/tile_grid.hpp"
#include "canvas/scheduling/runtime_scheduler.hpp"
#include "canvas/resources/resource_budget_manager.hpp"
#include <iostream>
#include <stdexcept>
#include <thread>
#include <atomic>
#include <vector>
#if __has_include("../src/gpu_resource_retirement.hpp")
#include "../src/gpu_resource_retirement.hpp"
#define HAS_RETIREMENT_QUEUE 1
#endif
using namespace canvas;
void require(bool ok,const char* s){if(!ok)throw std::runtime_error(s);}
template<class Manager>bool shrinkReservation(Manager& manager,std::uint64_t id,std::size_t bytes){
 if constexpr(requires{manager.shrink(id,bytes);})return manager.shrink(id,bytes);
 else return false;
}
template<class Lease>bool shrinkLease(Lease& lease,std::size_t bytes){
 if constexpr(requires{lease.shrink(bytes);})return lease.shrink(bytes);
 else return false;
}
void retirementBudget(){
 auto manager=std::make_shared<resources::ResourceBudgetManager>(resources::BudgetLimits{100,120});
 auto peak=manager->reserve(resources::ResourceClass::kTile,90,true);require(bool(peak),"retirement peak admission");
 resources::ResourceLease image(manager,*peak);
 require(shrinkLease(image,40),"retirement cannot shrink a live lease without readmission");
 require(manager->used()==40&&manager->used(resources::ResourceClass::kTile)==40,"staging retirement did not return quota");
 require(shrinkReservation(*manager,*peak,40),"shrink changed the reservation identity");
 require(!shrinkReservation(*manager,*peak,41)&&manager->used()==40,"shrink grew a reservation");
 require(!shrinkReservation(*manager,0,0),"invalid shrink token accepted");
 require(!manager->reserve(resources::ResourceClass::kRaster,70,false),"retirement bypassed soft admission");
 auto other=manager->reserve(resources::ResourceClass::kRaster,60,false);
 require(other&&*other==*peak+1&&manager->used()==100,"shrink released/readmitted the token or lost quota");
 auto moved=std::move(image);require(!shrinkLease(image,0),"moved-from lease changed quota");
 require(shrinkLease(moved,0)&&manager->used()==60,"zero shrink lost the reservation");
 require(shrinkReservation(*manager,*peak,0)&&!shrinkReservation(*manager,*peak,1),"zero reservation was released or grew");
 moved.reset();require(!shrinkReservation(*manager,*peak,0),"stale shrink token accepted");
 manager->release(*other);require(manager->used()==0,"retirement lease release leaked");
 auto concurrent=std::make_shared<resources::ResourceBudgetManager>(resources::BudgetLimits{100,120});
 std::atomic<bool> bad=false;std::vector<std::thread> threads;
 for(unsigned t=0;t<8;++t)threads.emplace_back([&,t]{
  for(unsigned i=0;i<1000;++i){
   const auto token=concurrent->reserve(resources::ResourceClass::kScratch,8+(i+t)%7,true);
   if(!token)continue;
   resources::ResourceLease lease(concurrent,*token);
   if(!shrinkLease(lease,1)||concurrent->used()>120)bad=true;
  }
 });
 for(auto& thread:threads)thread.join();
 require(!bad&&concurrent->used()==0,"concurrent GPU retirement corrupted hard quota or leaked");
}
template<class Manager,class Lease>void gpuEpochBudget(){
 if constexpr(requires(Manager& manager,Lease& lease){
    manager.beginGpuEpoch();manager.endGpuEpoch(1);manager.completeGpuEpoch(1);
    manager.pendingGpuEpochCount();manager.snapshot(resources::ResourceClass::kTile);
    manager.reserve(resources::ResourceClass::kTile,90,true,30);lease.completedShrink(30);
 }){
  auto manager=std::make_shared<Manager>(resources::BudgetLimits{100,120});
  auto peak=manager->reserve(resources::ResourceClass::kTile,90,true,30);
  require(bool(peak),"epoch image admission");Lease image(manager,*peak);
  auto finalShrink=image.completedShrink(30);
  const auto first=manager->beginGpuEpoch();require(bool(first),"first GPU epoch");
  require(!manager->beginGpuEpoch(),"nested GPU epoch accepted");
  require(!manager->completeGpuEpoch(*first),"unfinished GPU scope completed");
  const auto metadata=manager->reserve(resources::ResourceClass::kRecorded,20,true);
  require(bool(metadata),"CPU metadata admission during GPU scope");manager->release(*metadata);
  require(manager->used()==90,"CPU metadata incorrectly pinned to GPU scope");
  require(manager->endGpuEpoch(*first),"end first GPU epoch");
  require(!manager->endGpuEpoch(*first),"duplicate epoch end accepted");
  const auto second=manager->beginGpuEpoch();require(second&&manager->endGpuEpoch(*second),"final publishing epoch");
  require(manager->completeGpuEpoch(*first)&&manager->used()==90,"early callback retired live staging");
  require(finalShrink.apply()&&manager->used()==30,"final callback did not retire staging");
  require(manager->completeGpuEpoch(*second)&&manager->used()==30,"resident image lost its reservation");
  const auto stats=manager->snapshot(resources::ResourceClass::kTile);
  require(stats.used==30&&stats.staging==0&&stats.pendingRelease==0,"resident quota snapshot inconsistent");
  const auto read=manager->beginGpuEpoch();require(read&&manager->endGpuEpoch(*read),"resident image read epoch");
  image.reset();require(manager->used()==30&&manager->snapshot(resources::ResourceClass::kTile).pendingRelease==30,"eviction released unfinished GPU image");
  require(manager->completeGpuEpoch(*read)&&manager->used()==0,"finished eviction charge leaked");
  require(!manager->completeGpuEpoch(*read)&&!manager->completeGpuEpoch(0)&&!finalShrink.apply(),"stale GPU completion/shrink accepted");
  auto empty=manager->reserve(resources::ResourceClass::kTile,10,true,0);require(bool(empty),"empty retirement fixture");
  Lease emptyLease(manager,*empty);auto zero=emptyLease.completedShrink(0);
  auto emptyEpoch=manager->beginGpuEpoch();require(emptyEpoch&&manager->endGpuEpoch(*emptyEpoch),"empty retirement epoch");
  emptyLease.reset();require(zero.apply()&&manager->used()==0,"empty final retirement");
  require(manager->completeGpuEpoch(*emptyEpoch)&&!zero.apply(),"zero-byte deferred release leaked its token");
  auto a=manager->reserve(resources::ResourceClass::kRaster,40,true);require(bool(a),"context A resource");
  auto contextA=manager->beginGpuEpoch();require(contextA&&manager->endGpuEpoch(*contextA),"context A scope");
  auto b=manager->reserve(resources::ResourceClass::kRaster,30,true);require(bool(b),"context B resource");
  auto contextB=manager->beginGpuEpoch();require(contextB&&manager->endGpuEpoch(*contextB),"context B scope");
  manager->release(*a);manager->release(*b);
  require(manager->completeGpuEpoch(*contextB)&&manager->used()==70,"out-of-order completion freed older GPU uses");
  require(manager->completeGpuEpoch(*contextA)&&manager->used()==0,"ordered watermark did not drain releases");
  auto held=manager->beginGpuEpoch();require(held&&manager->endGpuEpoch(*held),"backlog head");
  unsigned admitted=1;
  for(unsigned i=0;i<2048;++i){auto epoch=manager->beginGpuEpoch();if(!epoch)break;
    require(manager->endGpuEpoch(*epoch)&&manager->completeGpuEpoch(*epoch),"empty queued GPU scope");++admitted;}
  require(admitted<=1024&&!manager->beginGpuEpoch(),"GPU retirement backlog unbounded");
  require(manager->completeGpuEpoch(*held)&&manager->pendingGpuEpochCount()==0,"backlog completion leaked epochs");
 }else require(false,"pixel reservations lack completion-bound GPU epochs");
}
template<class Manager,class Lease>void selectiveGpuUse(){
 if constexpr(requires(Manager& b,Lease& l){b.beginGpuEpoch(resources::kPixelResourceClasses,resources::ResourceClassMask{0});l.markGpuUse();}){
  auto b=std::make_shared<Manager>(resources::BudgetLimits{100,120});
  auto first=b->reserve(resources::ResourceClass::kNavigationTile,30,true);
  auto second=b->reserve(resources::ResourceClass::kNavigationTile,40,true);
  require(first&&second,"selective image admission");Lease sampled(b,*first),unused(b,*second);
  auto epoch=b->beginGpuEpoch(resources::kPixelResourceClasses,0);require(bool(epoch)&&sampled.markGpuUse(),"sampled image epoch stamp");
  unused.reset();require(b->used()==30,"unread navigation image was pinned by unrelated work");
  sampled.reset();require(b->used()==30,"sampled image freed before GPU completion");
  require(b->endGpuEpoch(*epoch)&&b->completeGpuEpoch(*epoch)&&b->used()==0,"sampled retirement leaked");
 }else require(false,"GPU uses cannot distinguish sampled images from eviction candidates");
}
template<class Queue>void canonicalSnapshotBudget(){
 constexpr auto allocations=[] {
  if constexpr(requires{Queue::canonicalAllocations;})return Queue::canonicalAllocations;
  else return resources::kPixelResourceClasses&~resources::resourceClassMask(resources::ResourceClass::kNavigationTile);
 }();
 constexpr auto existing=[] {
  if constexpr(requires{Queue::canonicalReads;})return Queue::canonicalReads;
  else return allocations;
 }();
 auto b=std::make_shared<resources::ResourceBudgetManager>(resources::BudgetLimits{100,120});
 auto prior=b->reserve(resources::ResourceClass::kNavigation,60,true);require(bool(prior),"prior snapshot admission");
 resources::ResourceLease oldSnapshot(b,*prior);
 auto epoch=b->beginGpuEpoch(allocations,existing);require(bool(epoch),"canonical write epoch");
 oldSnapshot.reset();require(b->used()==0,"canonical work pinned an unread prior navigation snapshot");
 auto next=b->reserve(resources::ResourceClass::kNavigation,80,true);require(bool(next),"unused prior snapshot blocked new capture");
 resources::ResourceLease newSnapshot(b,*next);newSnapshot.reset();
 require(b->used()==80,"new snapshot write released before GPU completion");
 require(b->endGpuEpoch(*epoch)&&b->completeGpuEpoch(*epoch)&&b->used()==0,"new snapshot completion leaked quota");
}
void completionOwnership(){
#if defined(HAS_RETIREMENT_QUEUE)
 struct Service final:render::GpuCompletionService {
  std::shared_ptr<render::GpuCompletionState> status=std::make_shared<render::GpuCompletionState>();
  unsigned polls=0;void service()noexcept override{++polls;}
  std::shared_ptr<const render::GpuCompletionState> state()const noexcept override{return status;}
 };
 auto budget=std::make_shared<resources::ResourceBudgetManager>(resources::BudgetLimits{4096,4096});
 render::internal::GpuResourceRetirement queue;
 auto service=std::make_shared<Service>();std::weak_ptr<Service> weak=service;
 auto denied=std::make_shared<resources::ResourceBudgetManager>(resources::BudgetLimits{32,32});
 require(!queue.begin(denied,service)&&denied->used()==0&&denied->pendingGpuEpochCount()==0,"completion metadata denial leaked an epoch or exceeded quota");
 auto token=budget->reserve(resources::ResourceClass::kNavigationTile,300,true,100);
 require(bool(token),"ticket image");resources::ResourceLease image(budget,*token);
 auto early=queue.begin(budget,service);require(bool(early),"ticket metadata/epoch admission");
 early->end();early->finish();queue.service();require(budget->used()==300,"early finish shrank staging");
 auto final=queue.begin(budget,service);require(bool(final)&&final->add(image.completedShrink(100)),"final permit");
 final->end();image.reset();require(budget->used()>=300,"eviction freed unfinished image");
 service.reset();queue.service();require(!weak.expired(),"logical provider replacement lost completion context");
 final->finish();queue.service();require(weak.expired()&&budget->used()==0,"completed context/ticket leaked");
 service=std::make_shared<Service>();token=budget->reserve(resources::ResourceClass::kNavigationTile,300,true,100);
 resources::ResourceLease orphan(budget,*token);auto abandoned=queue.begin(budget,service);
 require(abandoned&&abandoned->add(orphan.completedShrink(100)),"abandon ticket");abandoned->end();orphan.reset();
 service->status->abandoned=true;abandoned->finish();queue.service();
 require(budget->used()>=300&&budget->snapshot(resources::ResourceClass::kRecorded).used>0,"logical abandon released unfinished charge or epoch metadata");
 budget->close();queue.service();require(!budget->reserve(resources::ResourceClass::kRecorded,1,true)&&!budget->beginGpuEpoch(),"closed owner reused GPU credit");
 require(budget->used()==0,"closed owner leaked orphan charge");
 // Physical context loss discards storage; it is never a visible-frame receipt.
 budget=std::make_shared<resources::ResourceBudgetManager>(resources::BudgetLimits{4096,4096});
 service=std::make_shared<Service>();token=budget->reserve(resources::ResourceClass::kNavigationTile,300,true,100);
 resources::ResourceLease lost(budget,*token);auto discard=queue.begin(budget,service);require(bool(discard),"lost ticket");
 discard->end();lost.reset();service->status->physicalLost=true;service->status->abandoned=true;
 discard->finish();queue.service();require(budget->used()==0,"physical loss leaked derived charge");
#else
 require(false,"completion tickets lack provider-independent lifetime and abandon discrimination");
#endif
}
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
 retirementBudget();
 completionOwnership();
 canonicalSnapshotBudget<render::internal::GpuResourceRetirement>();
 selectiveGpuUse<resources::ResourceBudgetManager,resources::ResourceLease>();
 gpuEpochBudget<resources::ResourceBudgetManager,resources::ResourceLease>();
 std::cout<<"tile local validity, dirty-lazy work, scheduler dependency and budget guards\n";
}
