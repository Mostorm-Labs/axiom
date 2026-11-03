#include "canvas/render/render_group_manager.hpp"
#include <unordered_set>

namespace canvas::render {
RenderGroupManager::Update RenderGroupManager::update(std::span<const ReferenceTraversalEntry> sources,
    const foundation::WorldRect& clip,std::uint64_t resourceEpoch){
    Update result;std::unordered_set<foundation::ObjectId,foundation::ObjectIdHash> live;
    live.reserve(sources.size());std::size_t ordinal=0;
    for(const auto& source:sources){const auto id=source.record.objectId;live.insert(id);
        auto found=entries_.find(id);
        if(found==entries_.end()){
            entries_.emplace(id,Entry{RenderGroup{{id,1},{}},source,ordinal,resourceEpoch});
            result.contributions.push_back({id,std::nullopt,VisualContribution{source.record.worldBounds,source.contributesPixels,false}});
        }else{
            auto& old=found->second;const auto& a=old.source.record;const auto& b=source.record;
            const RenderGroupChange change{
                a.kind!=b.kind||a.kindVersion!=b.kindVersion||a.content!=b.content||old.source.command!=source.command||a.textLayout!=b.textLayout,
                a.transform!=b.transform,clip_!=clip,a.properties!=b.properties||a.eraseMasks!=b.eraseMasks||a.derivedGeometry!=b.derivedGeometry,
                old.ordinal!=ordinal||a.placement!=b.placement||old.source.contributesPixels!=source.contributesPixels,
                old.resourceEpoch!=resourceEpoch||a.directDependencies!=b.directDependencies,
                old.source.geometryChunked!=source.geometryChunked ||
                    old.source.geometryChunkIndices!=source.geometryChunkIndices};
            if(change.content||change.transform||change.clip||change.effect||change.order||change.resource||a.worldBounds!=b.worldBounds){
                result.contributions.push_back({id,VisualContribution{a.worldBounds,old.source.contributesPixels,false},VisualContribution{b.worldBounds,source.contributesPixels,false}});
                old.group.apply(change);old.source=source;
            }
            old.ordinal=ordinal;old.resourceEpoch=resourceEpoch;
        }
        ++ordinal;
    }
    for(auto it=entries_.begin();it!=entries_.end();)if(!live.contains(it->first)){
        result.contributions.push_back({it->first,VisualContribution{it->second.source.record.worldBounds,it->second.source.contributesPixels,false},std::nullopt});
        it=entries_.erase(it);
    }else ++it;
    clip_=clip;return result;
}
const RenderGroup* RenderGroupManager::find(foundation::ObjectId id)const noexcept {
    const auto found=entries_.find(id);return found==entries_.end()?nullptr:&found->second.group;
}
}
