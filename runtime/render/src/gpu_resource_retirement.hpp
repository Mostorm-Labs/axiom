#pragma once
#include "canvas/foundation/internal/g5_observation.hpp"
namespace g5 = canvas::foundation::internal::g5;
#include "canvas/render/skia_surface_provider.hpp"
#include "canvas/resources/resource_budget_manager.hpp"
#include <array>
#include <list>
#include <algorithm>
#if defined(AXIOM_ENABLE_FRAME_TIMING)
#include <cstdio>
#endif
namespace canvas::render::internal {
// GPU callbacks retain only quota/state, never provider, renderer or Runtime.
class GpuResourceRetirement final {
 public:
  ~GpuResourceRetirement(){
#if defined(AXIOM_G5_UR0_DIAGNOSTICS)
    for(const auto& e:entries_)g5::gpuTicketDiscarded(reinterpret_cast<std::uintptr_t>(e.ticket.get()),1);
#endif
  }
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
    // Standalone private queue callers already supply a qualified finished
    // callback. The production recording scope explicitly requires an outcome.
    std::atomic<int> submission{-1};
    std::atomic<bool> observed=false, resolved=false;
    std::atomic<bool> displayCancelled=false;
    bool unusedWork=false,requireSubmission=false;
    bool preparation=false;std::size_t count=0;
    std::array<resources::CompletedResourceShrink,8> shrink;
    bool add(resources::CompletedResourceShrink permit)noexcept {
      if(count==shrink.size())return false;shrink[count++]=std::move(permit);return true;
    }
    void end()noexcept{(void)budget->endGpuEpoch(epoch);}
    void resolve()noexcept {
      if(!observed.load(std::memory_order_acquire)||submission.load(std::memory_order_acquire)<0||resolved.exchange(true))return;
      const bool lost=state->physicalLost.load(std::memory_order_acquire);
      const bool abandoned=state->abandoned.load(std::memory_order_acquire)||submission.load()==0;
      if(!abandoned||lost)(void)budget->completeGpuEpoch(epoch);
      #if defined(AXIOM_G5_UR0_DIAGNOSTICS)
      if(!unusedWork)g5::gpuTicketCompleted(reinterpret_cast<std::uintptr_t>(this),abandoned,lost);
#endif
      done.store(true,std::memory_order_release);
    }
    void submitted(bool success)noexcept {submission.store(success?1:0,std::memory_order_release);resolve();}
    void expectSubmission()noexcept {requireSubmission=true;submission.store(-1,std::memory_order_release);}
    void finish()noexcept {
      if(!requireSubmission){int expected=-1;submission.compare_exchange_strong(expected,1);}
      observed.store(true,std::memory_order_release);resolve();
    }
    void unused()noexcept {
      unusedWork=true;submission.store(1);observed.store(true);resolve();
      G5_OBSERVE(g5::Kind::Counter,g5::Tag::resource_lease,false,20,0,reinterpret_cast<std::uintptr_t>(this),epoch,state->lifetime);
    }
  };
  std::shared_ptr<Ticket> begin(const std::shared_ptr<resources::ResourceBudgetManager>& budget,
      std::shared_ptr<GpuCompletionService> service,
      resources::ResourceClassMask mask=resources::kPixelResourceClasses,
      resources::ResourceClassMask existing=resources::kPixelResourceClasses){
    this->service();
    if(!budget||!service||entries_.size()>=1024)return {};
    const auto token=budget->reserve(resources::ResourceClass::kRecorded,sizeof(Ticket)+sizeof(Entry)+128,true);
    if(!token)return {};
    resources::ResourceLease metadata(budget,*token);
    auto ticket=std::make_shared<Ticket>();ticket->budget=budget;ticket->state=service->state();
    if(!ticket->state||ticket->state->abandoned.load())return {};
    // Allocate the queue row before entering the non-reentrant GPU scope.
    entries_.push_back({ticket,std::move(service)});
    const auto epoch=budget->beginGpuEpoch(mask,existing);
    if(!epoch){entries_.pop_back();return {};}
    ticket->epoch=*epoch;ticket->metadata=std::move(metadata);++admitted_;
    G5_OBSERVE(g5::Kind::Counter,g5::Tag::resource_lease,false,20,1,reinterpret_cast<std::uintptr_t>(ticket.get()),ticket->epoch,ticket->state->lifetime,entries_.size());
    return ticket;
  }
  void service()noexcept {
    if(servicing_)return;
    servicing_=true;
    std::array<const Entry*,1024> polled{};std::size_t count=0;
    for(auto& e:entries_){
      const auto& state=e.ticket->state;
      if(state->physicalLost.load(std::memory_order_acquire)){
        (void)e.ticket->budget->completeGpuEpoch(e.ticket->epoch);
      }
      if(e.ticket->done.load(std::memory_order_acquire))continue;
      if(state->abandoned.load(std::memory_order_acquire)){
        // Unknown abandon is terminal, not successful completion. Physical loss
        // is a separate fact and may release an otherwise orphaned epoch.
        if(state->physicalLost.load(std::memory_order_acquire)){
          int expected=-1;e.ticket->submission.compare_exchange_strong(expected,0);
          e.ticket->finish();
        }
        continue;
      }
      bool seen=false;
      for(std::size_t i=0;i<count;++i){const auto& old=polled[i]->ticket->state;
        if(old.get()==state.get()&&!old.owner_before(state)&&!state.owner_before(old)){seen=true;break;}}
      if(!seen){polled[count++]=&e;e.service->service();}
    }
    std::erase_if(entries_,[this](auto& e){if(!e.ticket->done.load(std::memory_order_acquire)||!e.ticket->budget->gpuEpochRetired(e.ticket->epoch))return false;
      if(!e.ticket->state->abandoned.load()&&e.ticket->submission.load()==1)
        for(std::size_t i=0;i<e.ticket->count;++i)(void)e.ticket->shrink[i].apply();
#if defined(AXIOM_G5_UR0_DIAGNOSTICS)
      g5::gpuTicketRetired(reinterpret_cast<std::uintptr_t>(e.ticket.get()),e.ticket->epoch);
#endif
      ++retired_;e.ticket->metadata.reset();return true;});
    G5_OBSERVE(g5::Kind::Counter,g5::Tag::resource_lease,false,20,2,count,entries_.size(),preparationCount());
#if defined(AXIOM_ENABLE_FRAME_TIMING)
    if(count||previousQueued_!=entries_.size()||previousRetired_!=retired_){
      std::size_t waiting=0,unknown=0,observed=0;
      for(const auto& e:entries_){waiting+=e.ticket->done.load()&&!e.ticket->budget->gpuEpochRetired(e.ticket->epoch);unknown+=e.ticket->state->abandoned.load()&&!e.ticket->state->physicalLost.load();observed+=e.ticket->observed.load();}
      std::fprintf(stderr,"AXIOM_GPU_RETIREMENT {\"owner\":\"%llu\",\"admitted\":\"%llu\",\"ordered_retired\":\"%llu\",\"queued\":%zu,\"polled_lifetimes\":%zu,\"observed\":%zu,\"waiting_ordered\":%zu,\"unknown_abandon\":%zu,\"needs_service\":%s}\n",static_cast<unsigned long long>(identity_),static_cast<unsigned long long>(admitted_),static_cast<unsigned long long>(retired_),entries_.size(),count,observed,waiting,unknown,pending()?"true":"false");
      previousQueued_=entries_.size();previousRetired_=retired_;
    }
#endif
    servicing_=false;
  }
  bool pending()const noexcept {
    return std::any_of(entries_.begin(),entries_.end(),[](const auto& e){
      return e.ticket->budget->gpuEpochRetired(e.ticket->epoch)||
        (!e.ticket->done.load()&&!e.ticket->state->abandoned.load()&&e.ticket->submission.load()!=0);
    });
  }
  bool blocked()const noexcept {
    return std::any_of(entries_.begin(),entries_.end(),[](const auto& e){return
      !e.ticket->budget->gpuEpochRetired(e.ticket->epoch)&&
      ((e.ticket->state->abandoned.load()&&!e.ticket->state->physicalLost.load())||e.ticket->submission.load()==0);
    });
  }
  void invalidateDisplays()noexcept {
    for(const auto& e:entries_)if(!e.ticket->displayCancelled.exchange(true)){
#if defined(AXIOM_G5_UR0_DIAGNOSTICS)
      g5::gpuTicketCancelled(reinterpret_cast<std::uintptr_t>(e.ticket.get()));
#endif
    }
  }
  std::size_t preparationCount()const noexcept {
    return std::count_if(entries_.begin(),entries_.end(),[](const auto& e){return e.ticket->preparation&&!e.ticket->done.load(std::memory_order_acquire);});
  }
 private:
  struct Entry{std::shared_ptr<Ticket> ticket;std::shared_ptr<GpuCompletionService> service;};
  std::list<Entry> entries_;
  bool servicing_=false;
  std::uint64_t admitted_=0,retired_=0;
#if defined(AXIOM_ENABLE_FRAME_TIMING)
  inline static std::atomic<std::uint64_t> nextIdentity_{1};
  const std::uint64_t identity_=nextIdentity_.fetch_add(1);
  std::size_t previousQueued_=0;
  std::uint64_t previousRetired_=0;
#endif
};
}
