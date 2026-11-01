#pragma once
#include <cstddef>
#include <algorithm>
#include <memory>
#include <cstdint>
#include <optional>
#include <unordered_map>

namespace canvas::resources {
enum class ResourceClass { kRecorded, kRaster, kScratch, kNavigation, kTile, kPromotion };
struct BudgetLimits { std::size_t soft=64U*1024U*1024U,hard=96U*1024U*1024U; };
// Derived resources only. Canonical storage cannot be admitted or evicted here.
class ResourceBudgetManager final {
 public:
  explicit ResourceBudgetManager(BudgetLimits limits={}):limits_(limits){}
  std::optional<std::uint64_t> reserve(ResourceClass kind,std::size_t bytes,bool critical){
    const auto cap=critical?limits_.hard:std::min(limits_.soft,limits_.hard);
    if(bytes>cap || used_>cap-bytes || next_==UINT64_MAX)return {};
    const auto id=++next_;entries_.emplace(id,Entry{kind,bytes});used_+=bytes;return id;
  }
  void release(std::uint64_t id)noexcept{auto it=entries_.find(id);if(it!=entries_.end()){used_-=it->second.bytes;entries_.erase(it);}}
  std::size_t used()const noexcept{return used_;}
  std::size_t used(ResourceClass kind)const noexcept{std::size_t n=0;for(const auto& [id,e]:entries_){(void)id;if(e.kind==kind)n+=e.bytes;}return n;}
 private:
  struct Entry{ResourceClass kind;std::size_t bytes;};
  BudgetLimits limits_;std::size_t used_=0;std::uint64_t next_=0;
  std::unordered_map<std::uint64_t,Entry> entries_;
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
  void reset()noexcept{if(owner_&&token_)owner_->release(token_);token_=0;owner_.reset();}
 private:std::shared_ptr<ResourceBudgetManager> owner_;std::uint64_t token_=0;
};
}
