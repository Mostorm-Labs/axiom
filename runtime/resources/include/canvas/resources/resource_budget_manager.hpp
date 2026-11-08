#pragma once
#if defined(AXIOM_G5_UR0_DIAGNOSTICS)
#include "canvas/foundation/internal/g5_observation.hpp"
#define AXIOM_G5_LEASE_OBSERVATIONS 1
#endif
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <unordered_map>

namespace canvas::resources {
enum class ResourceClass { kRecorded, kRaster, kScratch, kNavigation, kTile, kPromotion, kNavigationTile, kCanonicalFrame, kCount };
using ResourceClassMask = std::uint32_t;
constexpr ResourceClassMask resourceClassMask(ResourceClass kind) noexcept {
  return ResourceClassMask{1} << static_cast<unsigned>(kind);
}
constexpr ResourceClassMask kPixelResourceClasses = resourceClassMask(ResourceClass::kRaster) |
    resourceClassMask(ResourceClass::kScratch) | resourceClassMask(ResourceClass::kNavigation) |
    resourceClassMask(ResourceClass::kTile) | resourceClassMask(ResourceClass::kPromotion) |
    resourceClassMask(ResourceClass::kNavigationTile) | resourceClassMask(ResourceClass::kCanonicalFrame);
struct BudgetLimits { std::size_t soft=64U*1024U*1024U,hard=96U*1024U*1024U; };
struct ResourceBudgetUsage { std::size_t used=0,staging=0,pendingRelease=0; };
// Derived resources only. Canonical storage cannot be admitted or evicted here.
// GPU epochs are private rendering scopes, never presentation/commit receipts.
class ResourceBudgetManager final {
 public:
  explicit ResourceBudgetManager(BudgetLimits limits={}):limits_(limits){}
  std::optional<std::uint64_t> reserve(ResourceClass kind,std::size_t bytes,bool critical,
      std::size_t residentBytes=std::numeric_limits<std::size_t>::max()) {
    std::lock_guard lock(mutex_);
    if(residentBytes==std::numeric_limits<std::size_t>::max())residentBytes=bytes;
    const auto index=static_cast<std::size_t>(kind),cap=critical?limits_.hard:std::min(limits_.soft,limits_.hard);
    if(closed_||index>=classes_.size()||residentBytes>bytes||bytes>cap||used_>cap-bytes||next_==UINT64_MAX){
#if defined(AXIOM_G5_UR0_DIAGNOSTICS)
      G5_OBSERVE(foundation::internal::g5::Kind::Counter,foundation::internal::g5::Tag::resource_lease,false,7,observationOwner_,index,bytes,residentBytes,used_,cap,critical,closed_);
#endif
      return {};
    }
    const auto id=++next_;
    const auto epoch=activeGpuEpoch_&&(activeGpuClasses_&resourceClassMask(kind))?activeGpuEpoch_:0;
    entries_.emplace(id,Entry{kind,bytes,residentBytes,epoch,false});
    used_+=bytes;classes_[index].used+=bytes;classes_[index].staging+=bytes-residentBytes;
#if defined(AXIOM_G5_UR0_DIAGNOSTICS)
    G5_OBSERVE(foundation::internal::g5::Kind::Counter,foundation::internal::g5::Tag::resource_lease,false,1,observationOwner_,id,index,bytes,residentBytes,epoch,used_);
#endif
    return id;
  }
  // Caller must prove these bytes are no longer needed; a GPU publishing
  // scope applies its permit only from its actual finished callback.
  bool shrink(std::uint64_t id,std::size_t bytes)noexcept {
    std::lock_guard lock(mutex_);auto it=entries_.find(id);
    if(it==entries_.end()||bytes>it->second.bytes)return false;
    auto& e=it->second;auto& usage=classes_[static_cast<std::size_t>(e.kind)];
    const auto freed=e.bytes-bytes,oldStaging=e.bytes-e.residentBytes;
    e.bytes=bytes;e.residentBytes=std::min(e.residentBytes,bytes);
    used_-=freed;usage.used-=freed;usage.staging-=oldStaging-(e.bytes-e.residentBytes);
    if(e.pendingRelease)usage.pendingRelease-=freed;
#if defined(AXIOM_G5_UR0_DIAGNOSTICS)
    G5_OBSERVE(foundation::internal::g5::Kind::Counter,foundation::internal::g5::Tag::resource_lease,false,4,observationOwner_,id,static_cast<unsigned>(e.kind),e.bytes,e.residentBytes,e.gpuEpoch,used_);
#endif
    return true;
  }
  void release(std::uint64_t id)noexcept {
    std::lock_guard lock(mutex_);auto it=entries_.find(id);
    if(it==entries_.end()||it->second.pendingRelease)return;
#if defined(AXIOM_G5_UR0_DIAGNOSTICS)
    G5_OBSERVE(foundation::internal::g5::Kind::Counter,foundation::internal::g5::Tag::resource_lease,false,2,observationOwner_,id,static_cast<unsigned>(it->second.kind),it->second.bytes,it->second.residentBytes,it->second.gpuEpoch,used_);
#endif
    if(it->second.gpuEpoch>completedGpuEpoch_){
      it->second.pendingRelease=true;
      classes_[static_cast<std::size_t>(it->second.kind)].pendingRelease+=it->second.bytes;
      ++pendingReleaseCount_;
    }else erase(it);
  }
  std::size_t used()const noexcept {std::lock_guard lock(mutex_);return used_;}
  ResourceBudgetUsage snapshot(ResourceClass kind)const noexcept {
    std::lock_guard lock(mutex_);const auto index=static_cast<std::size_t>(kind);
    return index<classes_.size()?classes_[index]:ResourceBudgetUsage{};
  }
  std::size_t used(ResourceClass kind)const noexcept {return snapshot(kind).used;}
  // The owner must pre-admit completion metadata, end this non-reentrant scope
  // before flushing, and call complete only on actual GPU finish (or a scope
  // that recorded no GPU work). CPU Recorded metadata is deliberately excluded.
  std::optional<std::uint64_t> beginGpuEpoch(ResourceClassMask kinds=kPixelResourceClasses,ResourceClassMask existing=kPixelResourceClasses) {
    std::lock_guard lock(mutex_);
    if(closed_||activeGpuEpoch_||nextGpuEpoch_==UINT64_MAX||gpuEpochs_.size()>=1024)return {};
    const auto epoch=++nextGpuEpoch_;gpuEpochs_.emplace(epoch,GpuEpoch{});
    activeGpuEpoch_=epoch;activeGpuClasses_=kinds&kPixelResourceClasses;
    for(auto& [id,e]:entries_){(void)id;
      if(!e.pendingRelease&&(activeGpuClasses_&existing&resourceClassMask(e.kind)))e.gpuEpoch=epoch;
    }
    return epoch;
  }
  bool markGpuUse(std::uint64_t id)noexcept {
    std::lock_guard lock(mutex_);auto it=entries_.find(id);
    if(!activeGpuEpoch_||it==entries_.end()||it->second.pendingRelease)return false;
#if defined(AXIOM_G5_UR0_DIAGNOSTICS)
    const auto observationPreviousEpoch=it->second.gpuEpoch;
#endif
    it->second.gpuEpoch=activeGpuEpoch_;
#if defined(AXIOM_G5_UR0_DIAGNOSTICS)
    if(observationPreviousEpoch!=activeGpuEpoch_)G5_OBSERVE(foundation::internal::g5::Kind::Counter,foundation::internal::g5::Tag::resource_lease,false,5,observationOwner_,id,static_cast<unsigned>(it->second.kind),it->second.bytes,it->second.residentBytes,it->second.gpuEpoch,used_);
#endif
    return true;
  }
  bool endGpuEpoch(std::uint64_t epoch)noexcept {
    std::lock_guard lock(mutex_);const auto it=gpuEpochs_.find(epoch);
    if(epoch==0||activeGpuEpoch_!=epoch||it==gpuEpochs_.end())return false;
    it->second.ended=true;activeGpuEpoch_=0;activeGpuClasses_=0;return true;
  }
  bool completeGpuEpoch(std::uint64_t epoch)noexcept {
    std::lock_guard lock(mutex_);const auto it=gpuEpochs_.find(epoch);
    if(it==gpuEpochs_.end()||!it->second.ended||it->second.done)return false;
    it->second.done=true;const auto before=completedGpuEpoch_;
    while(!gpuEpochs_.empty()&&gpuEpochs_.begin()->second.ended&&gpuEpochs_.begin()->second.done){
      completedGpuEpoch_=gpuEpochs_.begin()->first;gpuEpochs_.erase(gpuEpochs_.begin());
    }
    if(before!=completedGpuEpoch_&&pendingReleaseCount_){
      for(auto entry=entries_.begin();entry!=entries_.end();){
        if(entry->second.pendingRelease&&entry->second.gpuEpoch<=completedGpuEpoch_)entry=erase(entry);
        else ++entry;
      }
    }
    return true;
  }
  // An owner shutdown can discard quota, never reuse it in a new frame.
  void close()noexcept {
    std::lock_guard lock(mutex_);closed_=true;activeGpuEpoch_=0;activeGpuClasses_=0;
    completedGpuEpoch_=nextGpuEpoch_;gpuEpochs_.clear();
    for(auto it=entries_.begin();it!=entries_.end();)if(it->second.pendingRelease)it=erase(it);else ++it;
  }
  bool gpuEpochRetired(std::uint64_t epoch)const noexcept {std::lock_guard lock(mutex_);return epoch&&epoch<=completedGpuEpoch_;}
  std::size_t pendingGpuEpochCount()const noexcept {std::lock_guard lock(mutex_);return gpuEpochs_.size();}
 private:
  struct Entry {ResourceClass kind;std::size_t bytes,residentBytes;std::uint64_t gpuEpoch;bool pendingRelease;};
  struct GpuEpoch {bool ended=false,done=false;};
  using Entries=std::unordered_map<std::uint64_t,Entry>;
  Entries::iterator erase(Entries::iterator it)noexcept {
    const auto& e=it->second;auto& usage=classes_[static_cast<std::size_t>(e.kind)];
#if defined(AXIOM_G5_UR0_DIAGNOSTICS)
    G5_OBSERVE(foundation::internal::g5::Kind::Counter,foundation::internal::g5::Tag::resource_lease,false,3,observationOwner_,it->first,static_cast<unsigned>(e.kind),e.bytes,e.residentBytes,e.gpuEpoch,used_);
#endif
    used_-=e.bytes;usage.used-=e.bytes;usage.staging-=e.bytes-e.residentBytes;
    if(e.pendingRelease){usage.pendingRelease-=e.bytes;--pendingReleaseCount_;}
    return entries_.erase(it);
  }
#if defined(AXIOM_G5_UR0_DIAGNOSTICS)
  const std::uint64_t observationOwner_=foundation::internal::g5::nextId();
#endif
  bool closed_=false;
  BudgetLimits limits_;std::size_t used_=0,pendingReleaseCount_=0;std::uint64_t next_=0;
  Entries entries_;
  std::array<ResourceBudgetUsage,static_cast<std::size_t>(ResourceClass::kCount)> classes_{};
  std::map<std::uint64_t,GpuEpoch> gpuEpochs_;
  std::uint64_t nextGpuEpoch_=0,activeGpuEpoch_=0,completedGpuEpoch_=0;
  ResourceClassMask activeGpuClasses_=0;
  mutable std::mutex mutex_;
};
class CompletedResourceShrink final {
 public:
  CompletedResourceShrink()=default;
  bool apply()const noexcept {return owner_&&token_&&owner_->shrink(token_,bytes_);}
 private:
  friend class ResourceLease;
  CompletedResourceShrink(std::shared_ptr<ResourceBudgetManager> owner,std::uint64_t token,std::size_t bytes)
      :owner_(std::move(owner)),token_(token),bytes_(bytes){}
  std::shared_ptr<ResourceBudgetManager> owner_;std::uint64_t token_=0;std::size_t bytes_=0;
};
class ResourceLease final {
 public:
  ResourceLease()=default;
  ResourceLease(std::shared_ptr<ResourceBudgetManager> owner,std::uint64_t token):owner_(std::move(owner)),token_(token){}
  ResourceLease(const ResourceLease&)=delete;
  ResourceLease& operator=(const ResourceLease&)=delete;
  ResourceLease(ResourceLease&& other)noexcept:owner_(std::move(other.owner_)),token_(other.token_){other.token_=0;}
  ResourceLease& operator=(ResourceLease&& other)noexcept{if(this!=&other){reset();owner_=std::move(other.owner_);token_=other.token_;other.token_=0;}return *this;}
  ~ResourceLease(){reset();}
  bool markGpuUse()const noexcept{return owner_&&token_&&owner_->markGpuUse(token_);}
  bool shrink(std::size_t bytes)noexcept{return owner_&&token_&&owner_->shrink(token_,bytes);}
  CompletedResourceShrink completedShrink(std::size_t bytes)const noexcept {return {owner_,token_,bytes};}
  void reset()noexcept{if(owner_&&token_)owner_->release(token_);token_=0;owner_.reset();}
 private:std::shared_ptr<ResourceBudgetManager> owner_;std::uint64_t token_=0;
};
}
