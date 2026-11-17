#pragma once
#include "source_retention.hpp"
#include "canvas/render/render_cache.hpp"
#include "canvas/foundation/internal/reference_validation_metrics.hpp"
#include <map>
namespace canvas::render::internal {
// Eligibility is supplied by the audited immutable FramePlan consumer or a
// generation-checked Scene query. It is not inferred from a pointer/refcount.
enum class HistoryValidationPolicy {kUntrusted,kPublishedImmutable};
class ReferenceValidationCache final {
 public:
    void bind(const RenderCacheOptions&,std::shared_ptr<resources::ResourceBudgetManager>,std::shared_ptr<SourceRetentionPool>) noexcept;
    bool bodyHit(const ReferenceTraversalEntry&) noexcept;
    void admitBody(const ReferenceTraversalEntry&) noexcept;
    bool historyHit(const scene::SharedVector<semantic::EraseMaskRecord>&,HistoryValidationPolicy) noexcept;
    void admitHistory(const scene::SharedVector<semantic::EraseMaskRecord>&,HistoryValidationPolicy) noexcept;
    void clear() noexcept;
    std::size_t size()const noexcept{return proofs_.size();}
    std::size_t metadataBytes()const noexcept{return metadataBytes_;}
    void reclaimFor(const std::shared_ptr<resources::ResourceBudgetManager>& budget)noexcept{if(budget_==budget)clear();}
 private:
    struct Less {bool operator()(const ImmutableSourceIdentity& a,const ImmutableSourceIdentity& b)const noexcept{
        AXIOM_VALIDATION_ADD(identityProbes,1);return ImmutableSourceIdentityLess{}(a,b);}};
    struct Proof {std::shared_ptr<const SourceRetention> source;resources::ResourceLease metadata;std::uint64_t used=0;};
    std::map<ImmutableSourceIdentity,Proof,Less> proofs_;
    std::shared_ptr<resources::ResourceBudgetManager> budget_;
    std::shared_ptr<SourceRetentionPool> pool_;
    RenderCacheOptions options_{};std::uint64_t tick_=0;std::size_t metadataBytes_=0;
    resources::ResourceLease domainMetadata_;
    bool domainReady_=false;
    static constexpr auto charge=sizeof(ImmutableSourceIdentity)+sizeof(Proof)+sizeof(void*)*8+256;
    static ImmutableSourceIdentity bodyKey(const ReferenceTraversalEntry&);
    static ImmutableSourceIdentity historyKey(const scene::SharedVector<semantic::EraseMaskRecord>&);
    bool hit(const ImmutableSourceIdentity&,bool body) noexcept;
    bool room() noexcept;
    bool ensureDomain() noexcept;
    void admit(ImmutableSourceIdentity,std::shared_ptr<const SourceRetention>) noexcept;
};
// The closure may outlive its renderer/helper. Both the actual reservation
// owner and the proof's current binding must still match the captured budget.
inline ValidationPressure validationPressure(std::weak_ptr<ReferenceValidationCache> proof,
    std::weak_ptr<resources::ResourceBudgetManager> owner)noexcept{
    try{return [proof,owner](const auto& requested){
        if(requested&&requested==owner.lock())if(auto live=proof.lock())live->reclaimFor(requested);
    };}catch(const std::bad_alloc&){return {};}
}
// Mandatory owner allocations get one identical retry after retiring only
// disposable validation proofs. Optional caches retain their existing fallback.
inline std::optional<std::uint64_t> reserveWithValidationReclaim(
    const std::shared_ptr<resources::ResourceBudgetManager>& budget,
    ReferenceValidationCache* proofs,resources::ResourceClass kind,std::size_t bytes,
    bool critical=true,std::size_t resident=std::numeric_limits<std::size_t>::max()){
    if(!budget)return {};
    auto token=budget->reserve(kind,bytes,critical,resident);
    if(!token&&critical&&proofs){proofs->reclaimFor(budget);token=budget->reserve(kind,bytes,critical,resident);}
    return token;
}
}
