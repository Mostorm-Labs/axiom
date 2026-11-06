#include "canvas/render/render_group_manager.hpp"
#include <unordered_set>
#include <cmath>

namespace canvas::render {
namespace {
bool addEraseDamage(RenderGroupManager::EraseDamage& damage,
    const semantic::EraseCubicSegment& segment,const semantic::Transform2D& transform) {
    const auto& a=segment.p0.position;const auto& b=segment.p1.position;
    const double radius=std::max(segment.p0.radius,segment.p1.radius);
    const double left=std::min({a.x,b.x,segment.control1.x,segment.control2.x})-radius;
    const double right=std::max({a.x,b.x,segment.control1.x,segment.control2.x})+radius;
    const double top=std::min({a.y,b.y,segment.control1.y,segment.control2.y})-radius;
    const double bottom=std::max({a.y,b.y,segment.control1.y,segment.control2.y})+radius;
    for(const auto& p:std::array<semantic::Vec2,4>{{{left,top},{right,top},{left,bottom},{right,bottom}}}) {
        const float x=static_cast<float>(transform.a*p.x+transform.c*p.y+transform.tx);
        const float y=static_cast<float>(transform.b*p.x+transform.d*p.y+transform.ty);
        if(!std::isfinite(x)||!std::isfinite(y))return false;
        if(!damage.bounds)damage.bounds=foundation::WorldRect{x,y,x,y};
        else {auto& bounds=*damage.bounds;bounds.left=std::min(bounds.left,x);bounds.top=std::min(bounds.top,y);bounds.right=std::max(bounds.right,x);bounds.bottom=std::max(bounds.bottom,y);}
    }
    return true;
}
bool appendEraseDamage(RenderGroupManager::EraseDamage& damage,
    const ReferenceTraversalEntry& previous,const ReferenceTraversalEntry& current) {
    if(previous.presentationEraseActive!=current.presentationEraseActive ||
       previous.presentationOpacity!=current.presentationOpacity)return false;
    const auto& before=previous.presentationEraseMasks;const auto& after=current.presentationEraseMasks;
    if(after.size()<before.size()||(!current.presentationEraseActive&&(!before.empty()||!after.empty())))return false;
    for(std::size_t i=0;i<after.size();++i) {
        const auto* sweep=std::get_if<semantic::SweptCircleMask>(&after[i].geometry);
        if(!sweep) {if(i>=before.size()||before[i]!=after[i])return false;continue;}
        std::size_t first=0;
        if(i<before.size()) {
            if(before[i].id!=after[i].id)return false;
            const auto* old=std::get_if<semantic::SweptCircleMask>(&before[i].geometry);if(!old)return false;
            first=old->segments.size();
            if(first>sweep->segments.size()||!std::equal(old->segments.begin(),old->segments.end(),sweep->segments.begin())) {
                // The first move replaces the Down tap with its first segment.
                if(first!=1||sweep->segments.empty())return false;
                const auto& tap=old->segments.front();
                if(tap.p0!=tap.p1||tap.control1!=tap.p0.position||tap.control2!=tap.p0.position||tap.p0!=sweep->segments.front().p0)return false;
                first=0;
            }
        }
        for(std::size_t j=first;j<sweep->segments.size();++j)
            if(!addEraseDamage(damage,sweep->segments[j],current.record.transform))return false;
    }
    return true;
}
}
RenderGroupManager::Update RenderGroupManager::update(std::span<const ReferenceTraversalEntry> sources,
    const foundation::WorldRect& clip,std::uint64_t resourceEpoch){
    Update result;result.erase.continuous=true;std::unordered_set<foundation::ObjectId,foundation::ObjectIdHash> live;
    live.reserve(sources.size());std::size_t ordinal=0;
    for(const auto& source:sources){const auto id=source.record.objectId;live.insert(id);
        result.erase.active|=source.presentationEraseActive;
        auto found=entries_.find(id);
        if(found==entries_.end()){
            result.erase.continuous=false;
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
            if(change.content||change.transform||change.clip||change.effect||change.order||change.resource||change.geometry||a.worldBounds!=b.worldBounds){
                result.erase.continuous=false;
                result.contributions.push_back({id,VisualContribution{a.worldBounds,old.source.contributesPixels,false},VisualContribution{b.worldBounds,source.contributesPixels,false}});
                old.group.apply(change);
            }
            if(!appendEraseDamage(result.erase,old.source,source))result.erase.continuous=false;
            // Rebind equal values too: the retained source must be the one
            // whose immutable allocation is covered by this frame's leases.
            old.source=source;
            old.ordinal=ordinal;old.resourceEpoch=resourceEpoch;
        }
        ++ordinal;
    }
    for(auto it=entries_.begin();it!=entries_.end();)if(!live.contains(it->first)){
        result.erase.continuous=false;
        result.contributions.push_back({it->first,VisualContribution{it->second.source.record.worldBounds,it->second.source.contributesPixels,false},std::nullopt});
        it=entries_.erase(it);
    }else ++it;
    clip_=clip;return result;
}
const RenderGroup* RenderGroupManager::find(foundation::ObjectId id)const noexcept {
    const auto found=entries_.find(id);return found==entries_.end()?nullptr:&found->second.group;
}
}
