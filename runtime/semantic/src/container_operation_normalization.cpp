#include "container_operation_internal.hpp"
#include "object_record_semantics_internal.hpp"
#include "canvas/semantic/canonical_numeric.hpp"
#include <algorithm>
namespace canvas::semantic::internal {
namespace {
template<class T,class Key> bool canonicalize(std::vector<T>& values,Key key) {
    if(values.size()>65535U)return false;
    std::sort(values.begin(),values.end(),[&](const auto& a,const auto& b){return key(a)<key(b);});
    return std::adjacent_find(values.begin(),values.end(),[&](const auto& a,const auto& b){return key(a)==key(b);})==values.end();
}
const auto id=[](const auto& v){return v.object_id;};
bool transform(Transform2D& t) {
    return normalizeFinite(t.a,t.a)&&normalizeFinite(t.b,t.b)&&normalizeFinite(t.c,t.c)&&normalizeFinite(t.d,t.d)&&normalizeFinite(t.tx,t.tx)&&normalizeFinite(t.ty,t.ty);
}
bool nodes(std::vector<ContainerStructuralExpectation>& values) {
    if(!canonicalize(values,id))return false;
    for(auto& v:values)if(!transform(v.transform_before))return false;
    return true;
}
}
bool normalizeContainerOperation(SetSectionContentOp& op) {
    if(!canonicalize(op.items,id)||!nodes(op.geometry_basis))return false;
    for(auto& i:op.items)if(!normalizeSectionContent(i.expected_content)||!normalizeSectionContent(i.replacement_content))return false;
    return true;
}
bool normalizeContainerOperation(EditContainerTopologyOp& op) {
    if(!nodes(op.guard.nodes)||!canonicalize(op.guard.child_sets,[](const auto& v){return v.parent_id;})||
       !canonicalize(op.relocations,id)||!canonicalize(op.order_only_edits,id))return false;
    for(auto& c:op.guard.child_sets)if(!canonicalize(c.complete_members,id))return false;
    for(auto& i:op.relocations)if(!transform(i.transform_after))return false;
    auto* a=std::get_if<IntroduceWrappersEffect>(&op.effect);
    auto* b=std::get_if<ReleaseWrappersEffect>(&op.effect);
    auto* records=a?&a->containers:b?&b->containers_before:nullptr;
    if(records) {
        if(!canonicalize(*records,[](const auto& v){return v.id;}))return false;
        for(auto& record:*records)if(!normalizeObjectRecord(record))return false;
    }
    return true;
}
} // namespace canvas::semantic::internal
