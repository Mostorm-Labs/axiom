#include "reference_validation_cache.hpp"
#include <algorithm>
namespace canvas::render::internal {
ImmutableSourceIdentity ReferenceValidationCache::bodyKey(const ReferenceTraversalEntry& e){
 return std::visit([&](const auto& c)->ImmutableSourceIdentity{
  using C=std::decay_t<decltype(c)>;
  if constexpr(std::is_same_v<C,CompactStrokeReferenceCommand>||std::is_same_v<C,VectorStrokeReferenceCommand>||
    std::is_same_v<C,DabStrokeReferenceCommand>||std::is_same_v<C,BrushStrokeReferenceCommand>||std::is_same_v<C,DabBrushStrokeReferenceCommand>){
    using T=std::remove_cvref_t<decltype(c.content.get())>;return {c.content.shared(),sourceTypeTag<T>(),e.command.index()};
  }else return {};
 },e.command);
}
ImmutableSourceIdentity ReferenceValidationCache::historyKey(const scene::SharedVector<semantic::EraseMaskRecord>& h){
 return {h.storage(),sourceTypeTag<std::vector<semantic::EraseMaskRecord>>(),ReferenceCommand{}.index()+std::variant_size_v<ReferenceCommand>};
}
void ReferenceValidationCache::bind(const RenderCacheOptions& o,std::shared_ptr<resources::ResourceBudgetManager> b,std::shared_ptr<SourceRetentionPool> p) noexcept{
 if(budget_!=b||pool_!=p||options_.enabled!=o.enabled||options_.maxBytes!=o.maxBytes||options_.maxEntryBytes!=o.maxEntryBytes||options_.maxEntries!=o.maxEntries)clear();
 options_=o;budget_=std::move(b);pool_=std::move(p);
}
void ReferenceValidationCache::clear() noexcept{AXIOM_VALIDATION_ADD(evictions,proofs_.size());proofs_.clear();metadataBytes_=0;domainMetadata_.reset();domainReady_=false;}
bool ReferenceValidationCache::ensureDomain() noexcept{
 if(domainReady_)return true;
 if(!options_.enabled||!budget_||!pool_)return false;
 try{const auto token=budget_->reserve(resources::ResourceClass::kRecorded,sizeof(*this)+128,false);if(!token)return false;
  domainMetadata_=resources::ResourceLease(budget_,*token);domainReady_=true;return true;
 }catch(const std::bad_alloc&){return false;}
}
bool ReferenceValidationCache::hit(const ImmutableSourceIdentity& key,bool body) noexcept{
 if(!options_.enabled||!budget_||!pool_||!key.storage)return false;
 const auto it=proofs_.find(key);if(it==proofs_.end()){AXIOM_VALIDATION_ADD(misses,1);return false;}
 it->second.used=++tick_;if(body)AXIOM_VALIDATION_ADD(bodyHits,1);else AXIOM_VALIDATION_ADD(historyHits,1);return true;
}
bool ReferenceValidationCache::bodyHit(const ReferenceTraversalEntry& e)noexcept{return hit(bodyKey(e),true);}
bool ReferenceValidationCache::historyHit(const scene::SharedVector<semantic::EraseMaskRecord>& h,HistoryValidationPolicy p)noexcept{
 return p==HistoryValidationPolicy::kPublishedImmutable&&!h.empty()&&hit(historyKey(h),false);
}
bool ReferenceValidationCache::room()noexcept{
 if(!options_.enabled||!budget_||!pool_||!options_.maxEntries||charge>std::min(options_.maxBytes,options_.maxEntryBytes)||!ensureDomain())return false;
 while(!proofs_.empty()&&(proofs_.size()>=options_.maxEntries||metadataBytes_>options_.maxBytes-charge)){
  const auto it=std::min_element(proofs_.begin(),proofs_.end(),[](const auto& a,const auto& b){return a.second.used<b.second.used;});
  proofs_.erase(it);metadataBytes_-=charge;AXIOM_VALIDATION_ADD(evictions,1);
 }return true;
}
void ReferenceValidationCache::admit(ImmutableSourceIdentity key,std::shared_ptr<const SourceRetention> source)noexcept{
 const auto cap=std::min(options_.maxEntryBytes,options_.maxBytes);
 if(!source||!key.storage||charge>cap||source->bytes>cap-charge){AXIOM_VALIDATION_ADD(denials,1);return;}
 try{
  auto token=budget_->reserve(resources::ResourceClass::kRecorded,charge,false);if(!token){AXIOM_VALIDATION_ADD(denials,1);return;}
  resources::ResourceLease lease(budget_,*token);
  if(proofs_.emplace(std::move(key),Proof{std::move(source),std::move(lease),++tick_}).second)metadataBytes_+=charge;
 }catch(const std::bad_alloc&){AXIOM_VALIDATION_ADD(denials,1);}
}
void ReferenceValidationCache::admitBody(const ReferenceTraversalEntry& e)noexcept{
 try{auto key=bodyKey(e);if(!key.storage||!room())return;
  const auto retained=std::visit([&](const auto& c)->std::shared_ptr<const SourceRetention>{
   if constexpr(requires{c.content;})return pool_->getContent(c.content,budget_,false);else return {};
  },e.command);admit(std::move(key),retained);
 }catch(const std::bad_alloc&){AXIOM_VALIDATION_ADD(denials,1);}
}
void ReferenceValidationCache::admitHistory(const scene::SharedVector<semantic::EraseMaskRecord>& h,HistoryValidationPolicy p)noexcept{
 if(p!=HistoryValidationPolicy::kPublishedImmutable||h.empty())return;
 try{if(room())admit(historyKey(h),pool_->getMasks(h,budget_,false));}catch(const std::bad_alloc&){AXIOM_VALIDATION_ADD(denials,1);}
}
}
