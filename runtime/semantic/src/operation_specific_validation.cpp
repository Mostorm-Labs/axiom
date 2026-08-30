#include "canvas/semantic/operation_specific_validation.hpp"

#include "canvas/semantic/connector_validation.hpp"
#include "canvas/semantic/hierarchy_capability_validation.hpp"
#include "canvas/semantic/hierarchy_validation.hpp"
#include "canvas/semantic/operation_state_validator.hpp"
#include "canvas/semantic/staged_object_view.hpp"
#include "rich_text_state_plan.hpp"

#include <algorithm>
#include <map>
#include <set>
#include <span>
#include <type_traits>
#include <utility>
#include <variant>

namespace canvas::semantic {
namespace {
StatefulResult fail(StatefulIssue i) { return StatefulResult{i}; }

StatefulResult hierarchyAndCaps(const StagedObjectView& staged, const std::vector<ObjectId>& ids,
                                const std::vector<HierarchyEdit>& edits) {
    auto h = validateStagedHierarchy(staged, std::span<const HierarchyEdit>(edits.data(), edits.size()));
    if (!h.ok()) return h;
    return validateStagedHierarchyCapabilities(staged, std::span<const ObjectId>(ids.data(), ids.size()));
}

template <typename Op, typename Out, typename Build>
StatefulResult replaceBatch(const Op& op, const ObjectStore& base, Out* out, Build&& build) {
    Out result;
    auto status = build(op, base, result);
    if (!status.ok()) return status;
    *out = std::move(result); return {};
}

} // namespace

StatefulResult validateInsertObjectsState(const InsertObjectsOp& op, const ObjectStore& base, CreateObjectsStatePlanInputs* out) {
    if (!out) return fail(StatefulIssue::kInvalidApplicability);
    CreateObjectsStatePlanInputs result; StagedObjectView staged(base);
    for (const auto& c : op.objects) if (!staged.stageCreate(c)) return fail(StatefulIssue::kObjectAlreadyExists);
    std::vector<HierarchyEdit> edits; std::vector<ObjectId> ids;
    for (const auto& c : op.objects) { auto r = validateRecordStateForOperation(c, StateRule::kCreateAbsent); if (!r.ok()) return r; edits.push_back({c.id,c.placement}); ids.push_back(c.id); }
    auto h = hierarchyAndCaps(staged, ids, edits); if (!h.ok()) return h;
    for (const auto& c : op.objects) if (c.kind == ObjectKind::kConnector) { const auto* x = std::get_if<ConnectorContent>(&c.content); if (!x) return fail(StatefulIssue::kInvalidApplicability); auto r=validateConnectorReferences(staged,*x); if(!r.ok()) return r; }
    result.creates = op.objects; *out = std::move(result); return {};
}

StatefulResult validateSetPlacementsState(const SetPlacementsOp& op, const ObjectStore& base, ReplaceObjectsStatePlanInputs* out) {
    if (!out) return fail(StatefulIssue::kInvalidApplicability);
    ReplaceObjectsStatePlanInputs result; StagedObjectView staged(base); std::vector<HierarchyEdit> edits; std::vector<ObjectId> ids;
    for (const auto& i : op.items) { const auto* c=staged.find(i.object_id); if(!c) return fail(StatefulIssue::kObjectMissing); auto r=validateRecordStateForOperation(*c,StateRule::kPlacementTarget); if(!r.ok()) return r; ObjectRecord n=*c; n.placement=i.placement; if(!staged.stageReplace(n)) return fail(StatefulIssue::kObjectMissing); result.replacements.push_back(n); edits.push_back({n.id,n.placement}); ids.push_back(n.id); }
    auto h=hierarchyAndCaps(staged,ids,edits); if(!h.ok()) return h; *out=std::move(result); return {};
}

StatefulResult validateSetTransformsState(const SetTransformsOp& op, const ObjectStore& base, ReplaceObjectsStatePlanInputs* out) {
    if (!out) return fail(StatefulIssue::kInvalidApplicability);
    ReplaceObjectsStatePlanInputs result;
    for (const auto& i:op.items){ const auto*c=base.find(i.object_id); if(!c)return fail(StatefulIssue::kObjectMissing); auto r=validateRecordStateForOperation(*c,StateRule::kTransformTarget);if(!r.ok())return r; auto n=*c;n.transform=i.transform;result.replacements.push_back(std::move(n)); }
    *out=std::move(result); return {};
}

StatefulResult validatePatchPropertiesState(const PatchPropertiesOp& op, const ObjectStore& base, ReplaceObjectsStatePlanInputs* out) {
    if (!out) return fail(StatefulIssue::kInvalidApplicability);
    std::map<ObjectId,ObjectRecord> work;
    for(const auto&p:op.patches){ auto it=work.find(p.object_id); if(it==work.end()){const auto*c=base.find(p.object_id);if(!c)return fail(StatefulIssue::kObjectMissing);it=work.emplace(p.object_id,*c).first;} auto& rec=it->second; const PropertyValue* val=nullptr; if(p.action==PropertyPatchAction::kSet){val=std::get_if<PropertyValue>(&p.value);if(!val)return fail(StatefulIssue::kInvalidApplicability);} else if(p.action!=PropertyPatchAction::kClear)return fail(StatefulIssue::kInvalidApplicability); auto r=requirePropertyApplicability(rec,p.field_id,val);if(!r.ok())return r; auto pit=std::find_if(rec.properties.entries.begin(),rec.properties.entries.end(),[&](const auto&e){return e.field_id==p.field_id;}); if(p.action==PropertyPatchAction::kClear){if(pit!=rec.properties.entries.end())rec.properties.entries.erase(pit);} else { bool is_default=false; if(p.field_id==1U)is_default=std::get<bool>(*val)==true; else if(p.field_id==2U)is_default=std::get<bool>(*val)==false; else if(p.field_id==3U)is_default=std::get<float>(*val)==1.0F; else if(p.field_id==4U)is_default=std::get<BlendModeValue>(*val)==BlendModeValue::kNormal; else if(p.field_id==0x100U)is_default=std::get<FillStyleValue>(*val)==FillStyleValue{NoFill{}}; else if(p.field_id==0x101U)is_default=std::get<StrokeStyleValue>(*val)==StrokeStyleValue{NoStroke{}}; else if(p.field_id==0x200U||p.field_id==0x201U)is_default=std::get<ConnectorDecorationValue>(*val)==ConnectorDecorationValue::kNone; if(is_default){if(pit!=rec.properties.entries.end())rec.properties.entries.erase(pit);} else {if(pit!=rec.properties.entries.end())pit->value=*val; else rec.properties.entries.push_back({p.field_id,*val});}} }
    ReplaceObjectsStatePlanInputs result;
    for(auto& [id,rec]:work){static_cast<void>(id);std::sort(rec.properties.entries.begin(),rec.properties.entries.end(),[](const auto&a,const auto&b){return a.field_id<b.field_id;});result.replacements.push_back(std::move(rec));}
    *out = std::move(result); return {};
}

StatefulResult validateSetObjectSizeState(const SetObjectSizeOp& op,const ObjectStore&base,ReplaceObjectsStatePlanInputs*out){if(!out)return fail(StatefulIssue::kInvalidApplicability);ReplaceObjectsStatePlanInputs r;for(const auto&i:op.items){const auto*c=base.find(i.object_id);if(!c)return fail(StatefulIssue::kObjectMissing);auto s=validateRecordStateForOperation(*c,StateRule::kSizeTarget);if(!s.ok())return s;auto n=*c;bool ok=false;std::visit([&](auto&x){using T=std::decay_t<decltype(x)>;if constexpr(std::is_same_v<T,ShapeContent>||std::is_same_v<T,ImageContent>||std::is_same_v<T,StickyContent>){x.width=i.width;x.height=i.height;ok=true;}},n.content);if(!ok)return fail(StatefulIssue::kInvalidApplicability);r.replacements.push_back(std::move(n));}*out=std::move(r);return{};}
StatefulResult validateSetVectorPathGeometryState(const SetVectorPathGeometryOp&op,const ObjectStore&base,ReplaceObjectsStatePlanInputs*out){if(!out)return fail(StatefulIssue::kInvalidApplicability);const auto*c=base.find(op.object_id);if(!c)return fail(StatefulIssue::kObjectMissing);auto s=validateRecordStateForOperation(*c,StateRule::kVectorPathTarget);if(!s.ok())return s;auto n=*c;auto*x=std::get_if<VectorPathContent>(&n.content);if(!x)return fail(StatefulIssue::kInvalidApplicability);x->geometry=op.geometry;out->replacements={std::move(n)};return{};}
StatefulResult validateSetImageContentState(const SetImageContentOp&op,const ObjectStore&base,ReplaceObjectsStatePlanInputs*out){if(!out)return fail(StatefulIssue::kInvalidApplicability);const auto*c=base.find(op.object_id);if(!c)return fail(StatefulIssue::kObjectMissing);auto s=validateRecordStateForOperation(*c,StateRule::kImageTarget);if(!s.ok())return s;auto n=*c;auto*x=std::get_if<ImageContent>(&n.content);if(!x)return fail(StatefulIssue::kInvalidApplicability);*x=op.content;out->replacements={std::move(n)};return{};}

StatefulResult validateAddStrokeState(const AddStrokeOp&op,const ObjectStore&base,CreateObjectsStatePlanInputs*out){if(!out)return fail(StatefulIssue::kInvalidApplicability);StagedObjectView staged(base);if(!staged.stageCreate(op.object))return fail(StatefulIssue::kObjectAlreadyExists);auto s=validateRecordStateForOperation(op.object,StateRule::kStrokeTarget);if(!s.ok())return s;std::vector<HierarchyEdit>e{{op.object.id,op.object.placement}};std::vector<ObjectId>ids{op.object.id};auto h=hierarchyAndCaps(staged,ids,e);if(!h.ok())return h;out->creates={op.object};return{};}

StatefulResult validateSplitStrokesState(const SplitStrokesOp&op,const ObjectStore&base,SplitStrokesStatePlanInputs*out){if(!out)return fail(StatefulIssue::kInvalidApplicability);StagedObjectView staged(base);for(const auto&s:op.splits){const auto*c=staged.find(s.source_stroke_id);if(!c)return fail(StatefulIssue::kObjectMissing);auto r=validateRecordStateForOperation(*c,StateRule::kStrokeTarget);if(!r.ok())return r;}std::vector<ObjectRecord> repl;for(const auto&s:op.splits)for(const auto&n:s.replacements){if(!staged.stageCreate(n))return fail(StatefulIssue::kObjectAlreadyExists);auto r=validateRecordStateForOperation(n,StateRule::kStrokeTarget);if(!r.ok())return r;repl.push_back(n);}for(const auto&s:op.splits)if(!staged.stageDelete(s.source_stroke_id))return fail(StatefulIssue::kObjectMissing);std::vector<HierarchyEdit>e;std::vector<ObjectId>ids;for(const auto&n:repl){e.push_back({n.id,n.placement});ids.push_back(n.id);}auto h=hierarchyAndCaps(staged,ids,e);if(!h.ok())return h;std::sort(repl.begin(),repl.end(),[](const auto&a,const auto&b){return a.id<b.id;});SplitStrokesStatePlanInputs result;for(const auto&s:op.splits)result.source_delete_ids.push_back(s.source_stroke_id);result.replacement_creates=std::move(repl);*out=std::move(result);return{};}

StatefulResult validateAddEraseMasksState(const AddEraseMasksOp&op,const ObjectStore&base,ReplaceObjectsStatePlanInputs*out){if(!out)return fail(StatefulIssue::kInvalidApplicability);ReplaceObjectsStatePlanInputs r;for(const auto&i:op.items){const auto*c=base.find(i.object_id);if(!c)return fail(StatefulIssue::kObjectMissing);auto s=validateRecordStateForOperation(*c,StateRule::kStrokeTarget);if(!s.ok())return s;auto n=*c;for(const auto&m:i.masks){if(std::any_of(n.erase_masks.begin(),n.erase_masks.end(),[&](const auto&x){return x.id==m.id; }))return fail(StatefulIssue::kMaskStateInvalid);n.erase_masks.push_back(m);}std::sort(n.erase_masks.begin(),n.erase_masks.end(),[](const auto&a,const auto&b){return a.id<b.id;});r.replacements.push_back(std::move(n));}*out=std::move(r);return{};}
StatefulResult validateRemoveEraseMasksState(const RemoveEraseMasksOp&op,const ObjectStore&base,ReplaceObjectsStatePlanInputs*out){if(!out)return fail(StatefulIssue::kInvalidApplicability);ReplaceObjectsStatePlanInputs r;for(const auto&i:op.items){const auto*c=base.find(i.object_id);if(!c)return fail(StatefulIssue::kObjectMissing);auto s=validateRecordStateForOperation(*c,StateRule::kStrokeTarget);if(!s.ok())return s;auto n=*c;for(const auto&id:i.mask_ids){auto it=std::find_if(n.erase_masks.begin(),n.erase_masks.end(),[&](const auto&m){return m.id==id;});if(it==n.erase_masks.end())return fail(StatefulIssue::kMaskStateInvalid);n.erase_masks.erase(it);}r.replacements.push_back(std::move(n));}*out=std::move(r);return{};}
StatefulResult validateEditRichTextState(const EditRichTextOp&op,const ObjectStore&base,ReplaceObjectsStatePlanInputs*out){if(!out)return fail(StatefulIssue::kInvalidApplicability);const auto*c=base.find(op.object_id);if(!c)return fail(StatefulIssue::kObjectMissing);auto s=validateRecordStateForOperation(*c,StateRule::kRichTextTarget);if(!s.ok())return s;const auto*x=std::get_if<RichTextContent>(&c->content);if(!x)return fail(StatefulIssue::kInvalidApplicability);RichTextContent next;auto r=internal::prepareRichTextDeltaState(*x,op.delta,&next);if(!r.ok())return r;auto n=*c;n.content=std::move(next);out->replacements={std::move(n)};return{};}
StatefulResult validateSetConnectorContentState(const SetConnectorContentOp&op,const ObjectStore&base,ReplaceObjectsStatePlanInputs*out){if(!out)return fail(StatefulIssue::kInvalidApplicability);const auto*c=base.find(op.object_id);if(!c)return fail(StatefulIssue::kObjectMissing);auto s=validateRecordStateForOperation(*c,StateRule::kConnectorTarget);if(!s.ok())return s;auto n=*c;n.content=op.content;StagedObjectView staged(base);if(!staged.stageReplace(n))return fail(StatefulIssue::kObjectMissing);auto r=validateConnectorReferences(staged,op.content);if(!r.ok())return r;out->replacements={std::move(n)};return{};}

} // namespace canvas::semantic
