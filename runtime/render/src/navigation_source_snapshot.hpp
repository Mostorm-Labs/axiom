#pragma once
#include "source_retention.hpp"
#include "canvas/render/render_cache.hpp"
#include <algorithm>
#include <memory>
#include <span>

namespace canvas::render::internal {
// Optional immutable comparison witness, not another Scene/document owner.
// Payload aliases share the existing retention pool; all copied metadata is
// admitted before allocation. Like the existing navigation source query, this
// proof supports already admitted pixels and uses critical recorded admission
// under the unchanged shared hard cap. Denial disables cross-Scene reuse only.
struct NavigationSourceSnapshot final {
    std::vector<ReferenceTraversalEntry> entries;
    SourceRetentions payloads;
    resources::ResourceLease metadata;
    static std::shared_ptr<const NavigationSourceSnapshot> capture(
        std::span<const ReferenceTraversalEntry> entries,const RenderCacheOptions& limits,
        const std::shared_ptr<resources::ResourceBudgetManager>& budget,
        const std::shared_ptr<SourceRetentionPool>& pool){
        if(!budget||!pool||entries.size()>limits.maxEntries)return {};
        const auto cap=std::min(limits.maxBytes,limits.maxEntryBytes);
        std::size_t bytes=sizeof(NavigationSourceSnapshot)+128;
        const auto add=[&](std::size_t n){if(bytes>cap||n>cap-bytes)return false;bytes+=n;return true;};
        if(!add(entries.size()*(sizeof(ReferenceTraversalEntry)+4*sizeof(SourceRetentions::value_type))))return {};
        for(const auto& e:entries){
            if(e.presentationEraseActive||!e.presentationEraseMasks.empty()||e.presentationOpacity!=1)return {};
            if(!add(sourceMetadataBytes(e))||!add(e.geometryChunkIndices.capacity()*sizeof(std::uint32_t)))return {};
        }
        const auto token=budget->reserve(resources::ResourceClass::kRecorded,bytes,true);if(!token)return {};
        resources::ResourceLease lease(budget,*token);
        auto result=std::make_shared<NavigationSourceSnapshot>();result->metadata=std::move(lease);
        result->payloads.reserve(entries.size()*4);
        for(const auto& e:entries){auto held=pool->get(e,budget,true);if(!held)return {};
            for(auto& p:*held)if(std::find(result->payloads.begin(),result->payloads.end(),p)==result->payloads.end())result->payloads.push_back(std::move(p));}
        result->entries.assign(entries.begin(),entries.end());return result;
    }
};
}
