#include "canvas/render/damage_tracker.hpp"
#include <algorithm>
#include <cmath>

namespace canvas::render {
namespace {
double unionArea(const std::vector<foundation::WorldRect>& rects) {
    std::vector<float> edges;
    for(const auto& r:rects){edges.push_back(r.left);edges.push_back(r.right);}
    std::sort(edges.begin(),edges.end());
    edges.erase(std::unique(edges.begin(),edges.end()),edges.end());
    double area=0;
    for(std::size_t i=1;i<edges.size();++i){
        std::vector<std::pair<float,float>> spans;
        for(const auto& r:rects)if(r.left<edges[i]&&r.right>edges[i-1])spans.emplace_back(r.top,r.bottom);
        std::sort(spans.begin(),spans.end());
        double height=0;float end=0;bool first=true;
        for(const auto& [top,bottom]:spans){height+=first?bottom-top:std::max(0.F,bottom-std::max(end,top));end=first?bottom:std::max(end,bottom);first=false;}
        area+=(double(edges[i])-edges[i-1])*height;
    }
    return area;
}
}
RenderDamage DamageTracker::collect(foundation::SceneRevision before,
    foundation::SceneRevision after,std::span<const VisualContributionDelta> changes,
    FullRedrawReason forced) const {
    RenderDamage result;result.before=before;result.after=after;result.reason=forced;
    if(after<before)result.reason=FullRedrawReason::kGenerationGap;
    for(const auto& change:changes){
        // Keep old and new extents distinct: a long move need not damage the
        // empty world between the two. Effects/clips use supplied visual extents.
        for(const auto* value:{&change.before,&change.after})if(*value&&(*value)->visible){
            const auto& contribution=**value;const auto& r=contribution.bounds;
            if(!r.isFiniteAndOrdered()||contribution.requiresBackdrop){result.reason=FullRedrawReason::kUnknownExtent;continue;}
            if(r.right>r.left&&r.bottom>r.top)result.rects.push_back(r);
        }
    }
    result.inputRectCount=result.rects.size();
    // The exact diagnostic is bounded. Large batches conservatively collapse;
    // exactUnionArea=-1 explicitly means unavailable, never an invented zero.
    result.exactUnionArea=result.rects.size()<=256?unionArea(result.rects):-1;
    if(result.rects.size()>maxRects_){auto bounds=result.rects.front();
        for(const auto& r:result.rects)bounds=foundation::unionRects(bounds,r);
        result.rects={bounds};
    }
    result.damageArea=unionArea(result.rects);
    result.fullRedraw=result.reason!=FullRedrawReason::kNone;
    return result;
}
RenderDamage DamageTracker::collect(const SceneDelta& delta) const {
    std::vector<VisualContributionDelta> contributions;
    contributions.reserve(delta.mutations.size());
    const auto visual=[](const SceneRecord& record){return VisualContribution{record.worldBounds,
        (static_cast<std::uint32_t>(record.flags)&static_cast<std::uint32_t>(SceneRecordFlags::kVisible))!=0,false};};
    for(const auto& mutation:delta.mutations){VisualContributionDelta change;change.objectId=mutation.objectId;
        if(mutation.before)change.before=visual(*mutation.before);
        if(mutation.after)change.after=visual(*mutation.after);
        contributions.push_back(std::move(change));
    }
    return collect(delta.generationFrom,delta.generationTo,contributions);
}
}
