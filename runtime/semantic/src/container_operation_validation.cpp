#include "container_operation_internal.hpp"
#include "object_record_semantics_internal.hpp"
#include <algorithm>
#include <cmath>
namespace canvas::semantic::internal {
namespace {
template<class T,class Key> bool keyed(const std::vector<T>& values,Key key) {
    if(values.size()>65535U)return false;
    for(std::size_t n=0;n<values.size();++n) {
        if(n&&!(key(values[n-1])<key(values[n])))return false;
    }
    return true;
}
const auto id=[](const auto& v){return v.object_id;};
bool placement(const Placement& p) {return p.order_key.isValid()&&(!p.parent_id||!p.parent_id->isZero());}
bool matrix(const Transform2D& t) {
    return std::isfinite(t.a)&&std::isfinite(t.b)&&std::isfinite(t.c)&&std::isfinite(t.d)&&std::isfinite(t.tx)&&std::isfinite(t.ty)&&(t.a*t.d-t.b*t.c)!=0.0;
}
bool knownVersion(ObjectKind k,std::uint32_t v) {
    const auto kind=static_cast<std::uint8_t>(k);
    return kind>=1U&&kind<=11U&&(v==1U||(k==ObjectKind::kShape&&(v==2U||v==3U))||
        (k==ObjectKind::kRichText&&(v==2U||v==3U))||(k==ObjectKind::kVectorStroke&&v==2U));
}
bool nodes(const std::vector<ContainerStructuralExpectation>& values) {
    if(!keyed(values,id))return false;
    for(const auto& v:values)if(v.object_id.isZero()||!knownVersion(v.kind,v.kind_version)||!placement(v.placement_before)||!matrix(v.transform_before))return false;
    return true;
}
}
bool validateContainerOperation(const SetSectionContentOp& op) noexcept {
    if(op.items.empty()||!keyed(op.items,id)||!nodes(op.geometry_basis))return false;
    for(const auto& i:op.items)if(i.object_id.isZero()||i.expected_kind_version!=1U||!validateSectionContent(i.expected_content)||!validateSectionContent(i.replacement_content))return false;
    return true;
}
bool validateContainerOperation(const EditContainerTopologyOp& op) noexcept {
    if(!nodes(op.guard.nodes)||!keyed(op.guard.child_sets,[](const auto& v){return v.parent_id;})||
       !keyed(op.relocations,id)||!keyed(op.order_only_edits,id)||std::holds_alternative<std::monostate>(op.effect))return false;
    for(const auto& c:op.guard.child_sets) {
        if((c.parent_id&&c.parent_id->isZero())||!keyed(c.complete_members,id))return false;
        for(const auto& m:c.complete_members)if(m.object_id.isZero()||!m.order_key.isValid())return false;
    }
    for(const auto& i:op.relocations)if(i.object_id.isZero()||!placement(i.placement_after)||!matrix(i.transform_after))return false;
    for(const auto& i:op.order_only_edits) {
        if(i.object_id.isZero()||!i.order_key_after.isValid())return false;
        if(std::binary_search(op.relocations.begin(),op.relocations.end(),ContainerRelocation{i.object_id,{},{}},[](const auto& a,const auto& b){return a.object_id<b.object_id;}))return false;
    }
    const auto* a=std::get_if<IntroduceWrappersEffect>(&op.effect);
    const auto* b=std::get_if<ReleaseWrappersEffect>(&op.effect);
    const auto* records=a?&a->containers:b?&b->containers_before:nullptr;
    if(records) {
        if(records->empty()||!keyed(*records,[](const auto& r){return r.id;}))return false;
        for(const auto& r:*records) {
            if((r.kind!=ObjectKind::kGroup&&r.kind!=ObjectKind::kSection)||!validateObjectRecord(r))return false;
            const auto move=std::lower_bound(op.relocations.begin(),op.relocations.end(),r.id,[](const auto& i,const ObjectId& key){return i.object_id<key;});
            const auto order=std::lower_bound(op.order_only_edits.begin(),op.order_only_edits.end(),r.id,[](const auto& i,const ObjectId& key){return i.object_id<key;});
            if((move!=op.relocations.end()&&move->object_id==r.id)||(order!=op.order_only_edits.end()&&order->object_id==r.id))return false;
        }
    } else if(op.relocations.empty())return false;
    return true;
}
} // namespace canvas::semantic::internal
