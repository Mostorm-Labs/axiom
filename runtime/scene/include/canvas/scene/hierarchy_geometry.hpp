#pragma once
#include "canvas/semantic/object_record.hpp"
#include "canvas/foundation/world_geometry.hpp"
#include <span>
#include <vector>
#include <algorithm>
#include <unordered_map>

namespace canvas::scene {
// Discardable world projection of the existing Placement forest. Canonical
// records, local transforms and membership remain owned by Semantic.
[[nodiscard]] std::vector<semantic::ObjectRecord> projectWorldObjects(
    std::span<const semantic::ObjectRecord>);
[[nodiscard]] semantic::Transform2D composeTransform(
    const semantic::Transform2D&, const semantic::Transform2D&) noexcept;
[[nodiscard]] bool inverseTransform(const semantic::Transform2D&,
                                    semantic::Transform2D&) noexcept;

template<class Record> void projectGroupBounds(std::vector<Record>& records) {
    const bool hierarchical=std::any_of(records.begin(),records.end(),[](const auto& r){return r.kind==semantic::ObjectKind::kSection||r.kind==semantic::ObjectKind::kGroup;});
    std::unordered_map<semantic::ObjectId,std::size_t,foundation::ObjectIdHash> indices;
    indices.reserve(records.size());
    std::vector<foundation::WorldRect> subtrees;subtrees.reserve(records.size());
    std::vector<bool> have(records.size(),false);
    for(std::size_t i=0;i<records.size();++i) {
        records[i].hierarchyOrder=hierarchical?i+1:0;
        indices.emplace(records[i].objectId,i);subtrees.push_back(records[i].worldBounds);
        have[i]=records[i].kind!=semantic::ObjectKind::kGroup;
    }
    for(std::size_t n=records.size();n>0;--n) {
        const auto i=n-1;auto it=records.begin()+static_cast<std::ptrdiff_t>(i);
        const auto bounds=subtrees[i];
        if(have[i]&&it->placement.parent_id) {
            const auto parent=indices.find(*it->placement.parent_id);
            if(parent!=indices.end()) {
                const auto p=parent->second;subtrees[p]=have[p]?foundation::unionRects(subtrees[p],bounds):bounds;have[p]=true;
            }
        }
        if(it->kind!=semantic::ObjectKind::kGroup||!have[i])continue;
        it->worldBounds=bounds;
        semantic::Transform2D inverse{};
        if(!inverseTransform(it->transform,inverse))continue;
        bool first=true;foundation::WorldRect local{};
        for(double x:{static_cast<double>(bounds.left),static_cast<double>(bounds.right)})
          for(double y:{static_cast<double>(bounds.top),static_cast<double>(bounds.bottom)}) {
            const foundation::WorldRect point{static_cast<float>((inverse.a*x+inverse.c*y)+inverse.tx),
                static_cast<float>((inverse.b*x+inverse.d*y)+inverse.ty),
                static_cast<float>((inverse.a*x+inverse.c*y)+inverse.tx),
                static_cast<float>((inverse.b*x+inverse.d*y)+inverse.ty)};
            local=first?point:foundation::unionRects(local,point);first=false;
          }
        it->geometryBounds=local;it->visualBounds=local;
    }
}
}
