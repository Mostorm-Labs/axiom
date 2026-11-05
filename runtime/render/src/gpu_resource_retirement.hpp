#pragma once
#include "canvas/render/skia_surface_provider.hpp"
#include "canvas/resources/resource_budget_manager.hpp"
#include <array>
#include <list>
namespace canvas::render::internal {
// GPU callbacks retain only quota/state, never provider, renderer or Runtime.
class GpuResourceRetirement final {
 public:
  // Canonical drawing can CREATE a new retained snapshot,but does not sample
  // the prior navigation image. Only navigation read scopes extend that use.
  static constexpr auto canonicalAllocations=resources::kPixelResourceClasses&
      ~resources::resourceClassMask(resources::ResourceClass::kNavigationTile);
  static constexpr auto canonicalReads=canonicalAllocations&
      ~resources::resourceClassMask(resources::ResourceClass::kNavigation)&
      ~resources::resourceClassMask(resources::ResourceClass::kCanonicalFrame);
  struct Ticket final {
    std::shared_ptr<resources::ResourceBudgetManager> budget;
    std::shared_ptr<const GpuCompletionState> state;
    resources::ResourceLease metadata;
    std::uint64_t epoch=0;std::atomic<bool> done=false;
    bool preparation=false;std::size_t count=0;
    std::array<resources::CompletedResourceShrink,8> shrink;
    bool add(resources::CompletedResourceShrink permit)noexcept {
      if(count==shrink.size())return false;shrink[count++]=std::move(permit);return true;
    }
    void end()noexcept{(void)budget->endGpuEpoch(epoch);}
    void finish()noexcept {
      // Abandon callbacks may run before physical GPU finish. Fail closed on
      // that path: retain orphaned charges until this budget owner closes.
      if(!state->abandoned.load(std::memory_order_acquire)){
        for(std::size_t i=0;i<count;++i)(void)shrink[i].apply();
        (void)budget->completeGpuEpoch(epoch);
      }else if(state->physicalLost.load(std::memory_order_acquire)){
        (void)budget->completeGpuEpoch(epoch);
      }
      done.store(true,std::memory_order_release);
    }
  };
  std::shared_ptr<Ticket> begin(const std::shared_ptr<resources::ResourceBudgetManager>& budget,
      std::shared_ptr<GpuCompletionService> service,
      resources::ResourceClassMask mask=resources::kPixelResourceClasses,
      resources::ResourceClassMask existing=resources::kPixelResourceClasses){
    if(!service||entries_.size()>=1024)return {};
    const auto token=budget->reserve(resources::ResourceClass::kRecorded,sizeof(Ticket)+sizeof(Entry)+128,true);
    if(!token)return {};
    resources::ResourceLease metadata(budget,*token);
    auto ticket=std::make_shared<Ticket>();ticket->budget=budget;ticket->state=service->state();
    if(!ticket->state||ticket->state->abandoned.load())return {};
    // Allocate the queue row before entering the non-reentrant GPU scope.
    entries_.push_back({ticket,std::move(service)});
    const auto epoch=budget->beginGpuEpoch(mask,existing);
    if(!epoch){entries_.pop_back();return {};}
    ticket->epoch=*epoch;ticket->metadata=std::move(metadata);return ticket;
  }
  void service()noexcept {
    for(auto& e:entries_)if(!e.ticket->done.load(std::memory_order_acquire))e.service->service();
    std::erase_if(entries_,[](auto& e){if(!e.ticket->done.load(std::memory_order_acquire)||!e.ticket->budget->gpuEpochRetired(e.ticket->epoch))return false;e.ticket->metadata.reset();return true;});
  }
  std::size_t preparationCount()const noexcept {
    return std::count_if(entries_.begin(),entries_.end(),[](const auto& e){return e.ticket->preparation&&!e.ticket->done.load(std::memory_order_acquire);});
  }
 private:
  struct Entry{std::shared_ptr<Ticket> ticket;std::shared_ptr<GpuCompletionService> service;};
  std::list<Entry> entries_;
};
}
