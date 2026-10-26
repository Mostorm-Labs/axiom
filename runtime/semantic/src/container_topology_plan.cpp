#include "container_operation_internal.hpp"
#include "object_record_semantics_internal.hpp"
#include "canvas/semantic/apply_plan.hpp"
#include "canvas/semantic/connector_validation.hpp"
#include "canvas/semantic/hierarchy_validation.hpp"
#include "canvas/semantic/hierarchy_capability_validation.hpp"
#include <algorithm>
#include <map>
#include <set>
namespace canvas::semantic::internal {
namespace {
using Parent=std::optional<ObjectId>;
bool matches(const ContainerStructuralExpectation& e,const ObjectRecord& r) {
    return e.object_id==r.id&&e.kind==r.kind&&e.kind_version==r.kind_version&&e.placement_before==r.placement&&e.transform_before==r.transform;
}
using Nodes=std::map<ObjectId,const ContainerStructuralExpectation*>;
bool readNodes(const std::vector<ContainerStructuralExpectation>& entries,const ObjectStore& base,Nodes& nodes) {
    for(const auto& e:entries) {
        const auto* r=base.find(e.object_id);
        if(!r||!matches(e,*r))return false;
        nodes.emplace(e.object_id,&e);
    }
    return true;
}
template<class View> bool pathCovered(Parent id,const View& view,const Nodes& guards,const std::set<ObjectId>& introduced={}) {
    std::set<ObjectId> seen;
    while(id) {
        if(!seen.insert(*id).second)return false;
        const auto* r=view.find(*id);
        if(!r||(!introduced.contains(*id)&&!guards.contains(*id)))return false;
        id=r->placement.parent_id;
    }
    return true;
}
bool validateResult(const StagedObjectView& view) {
    const auto records=view.allObjects();std::vector<HierarchyEdit> edits;std::vector<ObjectId> ids;
    std::set<std::pair<Parent,OrderKey>> sibling_keys;
    for(const auto& r:records) {
        if(!validateObjectRecord(r)||!sibling_keys.emplace(r.placement.parent_id,r.placement.order_key).second)return false;
        edits.push_back({r.id,r.placement});ids.push_back(r.id);
        if(const auto* connector=std::get_if<ConnectorContent>(&r.content))
            if(!validateConnectorReferences(view,*connector).ok())return false;
    }
    return validateStagedHierarchy(view,edits).ok()&&validateStagedHierarchyCapabilities(view,ids).ok();
}
StatefulResult conflict(){return {StatefulIssue::kInvalidApplicability};}
}
StatefulResult prepareContainerOperation(const SetSectionContentOp& op,const ObjectStore& base,PreparedApplyPlan& plan) {
    Nodes basis;
    if(!readNodes(op.geometry_basis,base,basis))return conflict();
    for(const auto& item:op.items) {
        const auto* r=base.find(item.object_id);
        if(!r)return {StatefulIssue::kObjectMissing};
        if(r->kind!=ObjectKind::kSection||r->kind_version!=item.expected_kind_version)return {StatefulIssue::kInvalidKindVersion};
        const auto* content=std::get_if<SectionContent>(&r->content);
        if(!content||*content!=item.expected_content)return conflict();
        if(content->boundary!=item.replacement_content.boundary&&!pathCovered(r->id,base,basis))return conflict();
        auto replacement=*r;replacement.content=item.replacement_content;
        plan.replacements.push_back(std::move(replacement));
    }
    return {};
}
StatefulResult prepareContainerOperation(const EditContainerTopologyOp& op,const ObjectStore& base,PreparedApplyPlan& plan) {
    Nodes guards;
    if(!readNodes(op.guard.nodes,base,guards))return conflict();
    std::map<Parent,const ContainerChildrenExpectation*> child_sets;
    for(const auto& e:op.guard.child_sets) {
        if(e.parent_id&&!base.contains(*e.parent_id))return conflict();
        auto actual=base.children(e.parent_id);
        std::sort(actual.begin(),actual.end(),[](const auto& a,const auto& b){return a.id<b.id;});
        if(actual.size()!=e.complete_members.size())return conflict();
        for(std::size_t i=0;i<actual.size();++i)
            if(actual[i].id!=e.complete_members[i].object_id||actual[i].placement.order_key!=e.complete_members[i].order_key)return conflict();
        child_sets.emplace(e.parent_id,&e);
    }
    const auto* introduce=std::get_if<IntroduceWrappersEffect>(&op.effect);
    const auto* release=std::get_if<ReleaseWrappersEffect>(&op.effect);
    std::set<ObjectId> introduced,released;
    StagedObjectView staged(base);std::set<Parent> affected;
    if(introduce)for(const auto& r:introduce->containers) {
        if(!staged.stageCreate(r))return {StatefulIssue::kObjectAlreadyExists};
        introduced.insert(r.id);plan.creates.push_back(r);affected.insert(r.placement.parent_id);
    }
    if(release)for(const auto& r:release->containers_before) {
        const auto* actual=base.find(r.id);
        if(!actual||*actual!=r)return conflict();
        released.insert(r.id);plan.deletes.push_back(r.id);affected.insert(r.placement.parent_id);affected.insert(r.id);
    }
    std::set<ObjectId> frontier;
    for(const auto& id:released)for(const auto& child:base.children(id))if(!released.contains(child.id))frontier.insert(child.id);
    if(release) {
        std::set<ObjectId> relocation_ids;
        for(const auto& i:op.relocations)relocation_ids.insert(i.object_id);
        if(relocation_ids!=frontier)return conflict();
    }
    std::map<ObjectId,ObjectRecord> replacements;
    for(const auto& i:op.relocations) {
        const auto* before=base.find(i.object_id);
        if(!before||!guards.contains(i.object_id))return conflict();
        if(introduce&&(!i.placement_after.parent_id||!introduced.contains(*i.placement_after.parent_id)))return conflict();
        affected.insert(before->placement.parent_id);affected.insert(i.placement_after.parent_id);
        auto after=*before;after.placement=i.placement_after;after.transform=i.transform_after;
        replacements.emplace(i.object_id,std::move(after));
    }
    for(const auto& i:op.order_only_edits) {
        const auto* before=base.find(i.object_id);
        if(!before||!guards.contains(i.object_id)||!affected.contains(before->placement.parent_id))return conflict();
        auto after=*before;after.placement.order_key=i.order_key_after;
        replacements.emplace(i.object_id,std::move(after));
    }
    for(const auto& [id,r]:replacements) {static_cast<void>(id);if(!staged.stageReplace(r))return conflict();plan.replacements.push_back(r);}
    for(const auto& id:released)if(!staged.stageDelete(id))return conflict();
    for(const auto& parent:affected)if((!parent||!introduced.contains(*parent))&&!child_sets.contains(parent))return conflict();
    for(const auto& i:op.relocations) {
        if(!pathCovered(i.object_id,base,guards)||!pathCovered(i.placement_after.parent_id,staged,guards,introduced))return conflict();
    }
    for(const auto& i:op.order_only_edits)if(!pathCovered(i.object_id,base,guards))return conflict();
    if(introduce)for(const auto& r:introduce->containers)if(!pathCovered(r.id,staged,guards,introduced))return conflict();
    if(release)for(const auto& r:release->containers_before)if(!pathCovered(r.placement.parent_id,base,guards))return conflict();
    return validateResult(staged)?StatefulResult{}:conflict();
}
} // namespace canvas::semantic::internal
