#include "canvas/scene/hierarchy_geometry.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <unordered_map>

namespace canvas::scene {
semantic::Transform2D composeTransform(const semantic::Transform2D& p,
                                      const semantic::Transform2D& c) noexcept {
    return {p.a*c.a+p.c*c.b,p.b*c.a+p.d*c.b,p.a*c.c+p.c*c.d,p.b*c.c+p.d*c.d,
            (p.a*c.tx+p.c*c.ty)+p.tx,(p.b*c.tx+p.d*c.ty)+p.ty};
}
bool inverseTransform(const semantic::Transform2D& t,semantic::Transform2D& out) noexcept {
    const double determinant=t.a*t.d-t.b*t.c;
    if(!std::isfinite(determinant)||determinant==0)return false;
    out={t.d/determinant,-t.b/determinant,-t.c/determinant,t.a/determinant,
         (t.c*t.ty-t.d*t.tx)/determinant,(t.b*t.tx-t.a*t.ty)/determinant};
    return std::isfinite(out.a)&&std::isfinite(out.b)&&std::isfinite(out.c)&&
        std::isfinite(out.d)&&std::isfinite(out.tx)&&std::isfinite(out.ty);
}
std::vector<semantic::ObjectRecord> projectWorldObjects(std::span<const semantic::ObjectRecord> source) {
    using Id=semantic::ObjectId;
    std::unordered_map<Id,std::size_t,foundation::ObjectIdHash> index;
    index.reserve(source.size());
    for(std::size_t i=0;i<source.size();++i)index.emplace(source[i].id,i);
    std::vector<std::vector<std::size_t>> children(source.size()+1);
    for(std::size_t i=0;i<source.size();++i) {
        auto parent=source[i].placement.parent_id;
        const auto found=parent?index.find(*parent):index.end();
        children[found==index.end()?source.size():found->second].push_back(i);
    }
    for(auto& siblings:children)std::sort(siblings.begin(),siblings.end(),[&](auto a,auto b){
        if(source[a].placement.order_key!=source[b].placement.order_key)
            return source[a].placement.order_key<source[b].placement.order_key;
        return source[a].id<source[b].id;
    });
    std::vector<semantic::ObjectRecord> result;result.reserve(source.size());
    std::vector<std::pair<std::size_t,semantic::Transform2D>> stack;
    for(auto it=children.back().rbegin();it!=children.back().rend();++it)stack.push_back({*it,{}});
    while(!stack.empty()) {
        const auto [i,parentWorld]=stack.back();stack.pop_back();
        auto record=source[i];record.transform=composeTransform(parentWorld,record.transform);
        bool visible=true,locked=false;auto parent=record.placement.parent_id;
        while(parent) {
            const auto found=index.find(*parent);if(found==index.end())break;
            const auto& ancestor=source[found->second];
            if(ancestor.kind==semantic::ObjectKind::kGroup)for(const auto& property:ancestor.properties.entries) {
                if(property.field_id==1U)if(const auto* value=std::get_if<bool>(&property.value))visible=visible&&*value;
                if(property.field_id==2U)if(const auto* value=std::get_if<bool>(&property.value))locked=locked||*value;
            }
            parent=ancestor.placement.parent_id;
        }
        for(const auto [field,value]:{std::pair{1U,visible},std::pair{2U,locked}}) {
            if((field==1U&&value)||(field==2U&&!value))continue;
            auto found=std::find_if(record.properties.entries.begin(),record.properties.entries.end(),[&](const auto& p){return p.field_id==field;});
            if(found==record.properties.entries.end())record.properties.entries.push_back({field,value});else found->value=value;
        }
        std::sort(record.properties.entries.begin(),record.properties.entries.end(),[](const auto& a,const auto& b){return a.field_id<b.field_id;});
        result.push_back(std::move(record));
        for(auto it=children[i].rbegin();it!=children[i].rend();++it)
            stack.push_back({*it,result.back().transform});
    }
    // Invalid forests are never silently published as a partial document.
    if(result.size()!=source.size()) {
        result.assign(source.begin(),source.end());
        for(auto& record:result)record.transform.tx=std::numeric_limits<double>::quiet_NaN();
    }
    return result;
}
}
