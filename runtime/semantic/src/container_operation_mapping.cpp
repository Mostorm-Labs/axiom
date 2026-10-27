#include "container_operation_mapping.hpp"
#if defined(CANVAS_SEMANTIC_PROTOBUF)
#include "protobuf_object_mapping.hpp"
#include "protobuf_owned_append.hpp"
#include "auditoryworks/axiom/v1/operation.pb.h"
#include <memory>
namespace canvas::semantic::internal {
namespace {
namespace p=auditoryworks::axiom::v1;
void putId(const ObjectId& id,p::Id128& out) {out.set_value(std::string(id.bytes.begin(),id.bytes.end()));}
bool getId(const p::Id128& in,ObjectId& id) {
    if(!in.has_value()||in.value().size()!=16U)return false;
    std::copy(in.value().begin(),in.value().end(),id.bytes.begin());return !id.isZero();
}
void putKey(const OrderKey& key,p::OrderKey& out){out.set_value(std::string(key.bytes().begin(),key.bytes().end()));}
bool getKey(const p::OrderKey& in,OrderKey& out) {
    if(!in.has_value())return false;
    out=OrderKey({in.value().begin(),in.value().end()});return out.isValid();
}
void putPlacement(const Placement& v,p::Placement& out){if(v.parent_id)putId(*v.parent_id,*out.mutable_parent_id());putKey(v.order_key,*out.mutable_order_key());}
bool getPlacement(const p::Placement& v,Placement& out) {
    if(!v.has_order_key()||!getKey(v.order_key(),out.order_key))return false;
    out.parent_id.reset();if(v.has_parent_id()){ObjectId id;if(!getId(v.parent_id(),id))return false;out.parent_id=id;}return true;
}
void putMatrix(const Transform2D& t,p::Transform2D& out) {out.set_a(t.a);out.set_b(t.b);out.set_c(t.c);out.set_d(t.d);out.set_tx(t.tx);out.set_ty(t.ty);}
bool getMatrix(const p::Transform2D& t,Transform2D& out) {
    if(!t.has_a()||!t.has_b()||!t.has_c()||!t.has_d()||!t.has_tx()||!t.has_ty())return false;
    out={t.a(),t.b(),t.c(),t.d(),t.tx(),t.ty()};return true;
}
void putNode(const ContainerStructuralExpectation& n,p::ContainerStructuralExpectation& out) {
    putId(n.object_id,*out.mutable_object_id());out.set_kind_id(static_cast<std::uint32_t>(n.kind));out.set_kind_version(n.kind_version);
    putPlacement(n.placement_before,*out.mutable_placement_before());putMatrix(n.transform_before,*out.mutable_transform_before());
}
bool getNode(const p::ContainerStructuralExpectation& n,ContainerStructuralExpectation& out) {
    if(!n.has_object_id()||!getId(n.object_id(),out.object_id)||!n.has_kind_id()||n.kind_id()>255U||!n.has_kind_version()||
       !n.has_placement_before()||!getPlacement(n.placement_before(),out.placement_before)||!n.has_transform_before()||!getMatrix(n.transform_before(),out.transform_before))return false;
    out.kind=static_cast<ObjectKind>(n.kind_id());out.kind_version=n.kind_version();return true;
}
}
bool toProtobufContainerOperation(const OperationPayload& value,p::OperationPayload& destination) {
    if(const auto* section=std::get_if<SetSectionContentOp>(&value)) {
        auto owned=std::make_unique<p::SetSectionContentOp>(); auto* out=owned.get();
        for(const auto& i:section->items) {
            auto* item=appendOwned(out->mutable_items());putId(i.object_id,*item->mutable_object_id());item->set_expected_kind_version(i.expected_kind_version);
            if(!toProtobufSectionContent(i.expected_content,*item->mutable_expected_content())||!toProtobufSectionContent(i.replacement_content,*item->mutable_replacement_content()))return false;
        }
        for(const auto& n:section->geometry_basis)putNode(n,*appendOwned(out->mutable_geometry_basis()));
        destination.set_allocated_set_section_content(owned.release());
        return true;
    }
    const auto* topology=std::get_if<EditContainerTopologyOp>(&value);if(!topology)return false;
    auto owned=std::make_unique<p::EditContainerTopologyOp>(); auto* out=owned.get();auto* guard=out->mutable_guard();
    for(const auto& n:topology->guard.nodes)putNode(n,*appendOwned(guard->mutable_nodes()));
    for(const auto& c:topology->guard.child_sets) {
        auto* children=appendOwned(guard->mutable_child_sets());if(c.parent_id)putId(*c.parent_id,*children->mutable_parent_id());
        for(const auto& m:c.complete_members){auto* member=appendOwned(children->mutable_complete_members());putId(m.object_id,*member->mutable_object_id());putKey(m.order_key,*member->mutable_order_key());}
    }
    for(const auto& r:topology->relocations){auto* move=appendOwned(out->mutable_relocations());putId(r.object_id,*move->mutable_object_id());putPlacement(r.placement_after,*move->mutable_placement_after());putMatrix(r.transform_after,*move->mutable_transform_after());}
    for(const auto& r:topology->order_only_edits){auto* edit=appendOwned(out->mutable_order_only_edits());putId(r.object_id,*edit->mutable_object_id());putKey(r.order_key_after,*edit->mutable_order_key_after());}
    if(const auto* a=std::get_if<IntroduceWrappersEffect>(&topology->effect)) {
        auto effect=std::make_unique<p::IntroduceWrappersEffect>();for(const auto& r:a->containers)if(!toProtobufObjectRecord(r,*appendOwned(effect->mutable_containers())))return false;out->set_allocated_introduce_wrappers(effect.release());
    } else if(const auto* b=std::get_if<ReleaseWrappersEffect>(&topology->effect)) {
        auto effect=std::make_unique<p::ReleaseWrappersEffect>();for(const auto& r:b->containers_before)if(!toProtobufObjectRecord(r,*appendOwned(effect->mutable_containers_before())))return false;out->set_allocated_release_wrappers(effect.release());
    } else if(std::holds_alternative<RelocateObjectsEffect>(topology->effect))out->set_allocated_relocate_objects(std::make_unique<p::RelocateObjectsEffect>().release());
    else return false;
    destination.set_allocated_edit_container_topology(owned.release());
    return true;
}
bool fromProtobufContainerOperation(const p::OperationPayload& source,OperationPayload& destination) {
    if(source.has_set_section_content()) {
        SetSectionContentOp value;const auto& in=source.set_section_content();
        for(const auto& i:in.items()) {
            SectionContentEdit item;
            if(!i.has_object_id()||!getId(i.object_id(),item.object_id)||!i.has_expected_kind_version()||!i.has_expected_content()||!i.has_replacement_content()||
               !fromProtobufSectionContent(i.expected_content(),item.expected_content)||!fromProtobufSectionContent(i.replacement_content(),item.replacement_content))return false;
            item.expected_kind_version=i.expected_kind_version();value.items.push_back(std::move(item));
        }
        for(const auto& n:in.geometry_basis()){ContainerStructuralExpectation node;if(!getNode(n,node))return false;value.geometry_basis.push_back(std::move(node));}
        destination=std::move(value);return true;
    }
    if(!source.has_edit_container_topology())return false;
    const auto& in=source.edit_container_topology();if(!in.has_guard())return false;
    EditContainerTopologyOp value;
    for(const auto& n:in.guard().nodes()){ContainerStructuralExpectation node;if(!getNode(n,node))return false;value.guard.nodes.push_back(std::move(node));}
    for(const auto& c:in.guard().child_sets()) {
        ContainerChildrenExpectation children;
        if(c.has_parent_id()){ObjectId id;if(!getId(c.parent_id(),id))return false;children.parent_id=id;}
        for(const auto& m:c.complete_members()){ContainerChildExpectation member;if(!m.has_object_id()||!getId(m.object_id(),member.object_id)||!m.has_order_key()||!getKey(m.order_key(),member.order_key))return false;children.complete_members.push_back(std::move(member));}
        value.guard.child_sets.push_back(std::move(children));
    }
    for(const auto& r:in.relocations()){ContainerRelocation move;if(!r.has_object_id()||!getId(r.object_id(),move.object_id)||!r.has_placement_after()||!getPlacement(r.placement_after(),move.placement_after)||!r.has_transform_after()||!getMatrix(r.transform_after(),move.transform_after))return false;value.relocations.push_back(std::move(move));}
    for(const auto& r:in.order_only_edits()){ContainerOrderOnlyEdit edit;if(!r.has_object_id()||!getId(r.object_id(),edit.object_id)||!r.has_order_key_after()||!getKey(r.order_key_after(),edit.order_key_after))return false;value.order_only_edits.push_back(std::move(edit));}
    switch(in.effect_case()) {
    case p::EditContainerTopologyOp::kIntroduceWrappers:{IntroduceWrappersEffect e;for(const auto& r:in.introduce_wrappers().containers()){ObjectRecord object;if(!fromProtobufObjectRecord(r,object))return false;e.containers.push_back(std::move(object));}value.effect=std::move(e);break;}
    case p::EditContainerTopologyOp::kReleaseWrappers:{ReleaseWrappersEffect e;for(const auto& r:in.release_wrappers().containers_before()){ObjectRecord object;if(!fromProtobufObjectRecord(r,object))return false;e.containers_before.push_back(std::move(object));}value.effect=std::move(e);break;}
    case p::EditContainerTopologyOp::kRelocateObjects:value.effect=RelocateObjectsEffect{};break;
    case p::EditContainerTopologyOp::EFFECT_NOT_SET:return false;
    }
    destination=std::move(value);return true;
}
} // namespace canvas::semantic::internal
#endif
