#include "canvas/semantic/apply_plan.hpp"

#include "canvas/semantic/idempotency.hpp"
#include "canvas/semantic/staged_object_view.hpp"
#include "sha256_internal.hpp"

#include <algorithm>
#include <limits>
#include <utility>
#include <variant>

namespace canvas::semantic {
namespace {
PrepareResult rejected(StatefulResult e) { return PrepareResult{PrepareDisposition::kRejected, e, std::nullopt}; }

bool referencesResource(const ObjectRecord& object, const ResourceId& resource) {
    bool found = false;
    std::visit([&](const auto& content) {
        using T = std::decay_t<decltype(content)>;
        if constexpr (std::is_same_v<T, ImageContent>) {
            found = content.resource_id == resource;
        } else if constexpr (std::is_same_v<T, RichTextContent>) {
            for (const auto& paragraph : content.document.paragraphs) {
                for (const auto& run : paragraph.runs) {
                    if (run.style.font_resource_id && *run.style.font_resource_id == resource) {
                        found = true;
                    }
                }
            }
        } else if constexpr (std::is_same_v<T, VectorStrokeContent> || std::is_same_v<T, DabStrokeContent>) {
            found = content.stroke.brush.texture_resource_id && *content.stroke.brush.texture_resource_id == resource;
        } else if constexpr (std::is_same_v<T, BrushStrokeContent> || std::is_same_v<T, DabBrushStrokeContent>) {
            for (const auto& binding : content.stroke.snapshot.resources) {
                if (binding.resource_id == resource.value) {
                    found = true;
                }
            }
        }
    }, object.content);
    return found;
}

}

PrepareResult prepareApplyPlan(const Operation& operation, const StatefulValidationContext& context) {
    const auto idempotency = classifyOperation(operation, context.applied_operations);
    if (idempotency.disposition == IdempotencyDisposition::kAlreadyApplied)
        return {PrepareDisposition::kAlreadyApplied, {}, std::nullopt};
    if (idempotency.disposition == IdempotencyDisposition::kCollision)
        return rejected({StatefulIssue::kOperationIdCollision});

    PreparedApplyPlan plan;
    plan.operation = operation;
    StatefulResult status{};
    std::visit([&](const auto& payload) {
        using T = std::decay_t<decltype(payload)>;
        if constexpr (std::is_same_v<T, DeleteObjectsOp>) {
            StagedObjectView staged(context.objects);
            DeleteClosure closure;
            status = resolveDeleteClosure(staged, payload.object_ids, &closure);
            if (status.ok()) { plan.deletes = closure.final_delete_set; plan.delete_closure = std::move(closure); }
        } else if constexpr (std::is_same_v<T, RestoreObjectsOp>) {
            RestorePlanInputs out; status = validateRestoreObjects(payload, context.objects, &out); if(status.ok()) plan.creates=std::move(out.creates);
        } else if constexpr (std::is_same_v<T, InsertObjectsOp>) {
            CreateObjectsStatePlanInputs out; status=validateInsertObjectsState(payload,context.objects,&out); if(status.ok()) plan.creates=std::move(out.creates);
        } else if constexpr (std::is_same_v<T, AddStrokeOp>) {
            CreateObjectsStatePlanInputs out; status=validateAddStrokeState(payload,context.objects,&out); if(status.ok()) plan.creates=std::move(out.creates);
        } else if constexpr (std::is_same_v<T, SplitStrokesOp>) {
            SplitStrokesStatePlanInputs out; status=validateSplitStrokesState(payload,context.objects,&out); if(status.ok()){plan.deletes=std::move(out.source_delete_ids); plan.creates=std::move(out.replacement_creates);}
        } else if constexpr (std::is_same_v<T, SetPlacementsOp>) { ReplaceObjectsStatePlanInputs out; status=validateSetPlacementsState(payload,context.objects,&out); if(status.ok()) plan.replacements=std::move(out.replacements);
        } else if constexpr (std::is_same_v<T, SetTransformsOp>) { ReplaceObjectsStatePlanInputs out; status=validateSetTransformsState(payload,context.objects,&out); if(status.ok()) plan.replacements=std::move(out.replacements);
        } else if constexpr (std::is_same_v<T, PatchPropertiesOp>) { ReplaceObjectsStatePlanInputs out; status=validatePatchPropertiesState(payload,context.objects,&out); if(status.ok()) plan.replacements=std::move(out.replacements);
        } else if constexpr (std::is_same_v<T, SetObjectSizeOp>) { ReplaceObjectsStatePlanInputs out; status=validateSetObjectSizeState(payload,context.objects,&out); if(status.ok()) plan.replacements=std::move(out.replacements);
        } else if constexpr (std::is_same_v<T, SetVectorPathGeometryOp>) { ReplaceObjectsStatePlanInputs out; status=validateSetVectorPathGeometryState(payload,context.objects,&out); if(status.ok()) plan.replacements=std::move(out.replacements);
        } else if constexpr (std::is_same_v<T, SetImageContentOp>) { ReplaceObjectsStatePlanInputs out; status=validateSetImageContentState(payload,context.objects,&out); if(status.ok()) plan.replacements=std::move(out.replacements);
        } else if constexpr (std::is_same_v<T, AddEraseMasksOp>) { ReplaceObjectsStatePlanInputs out; status=validateAddEraseMasksState(payload,context.objects,&out); if(status.ok()) plan.replacements=std::move(out.replacements);
        } else if constexpr (std::is_same_v<T, RemoveEraseMasksOp>) { ReplaceObjectsStatePlanInputs out; status=validateRemoveEraseMasksState(payload,context.objects,&out); if(status.ok()) plan.replacements=std::move(out.replacements);
        } else if constexpr (std::is_same_v<T, EditRichTextOp>) { ReplaceObjectsStatePlanInputs out; status=validateEditRichTextStateForDocument(payload,context.objects,&out,operation.document_id); if(status.ok()) plan.replacements=std::move(out.replacements);
        } else if constexpr (std::is_same_v<T, SetConnectorContentOp>) { ReplaceObjectsStatePlanInputs out; status=validateSetConnectorContentState(payload,context.objects,&out); if(status.ok()) plan.replacements=std::move(out.replacements); }
        else if constexpr (std::is_same_v<T, UpdateResourceManifestOp>) {
            if (context.manifest == nullptr || !validateResourceManifest(*context.manifest) || payload.expected_manifest_revision != context.manifest->manifest_revision) { status = {StatefulIssue::kInvalidApplicability}; return; }
            std::vector<ResourceId> action_ids;
            for (const auto& entry : payload.upserts) action_ids.push_back(entry.resource_id);
            action_ids.insert(action_ids.end(), payload.removes.begin(), payload.removes.end());
            for (const auto& reactivation : payload.reactivations) action_ids.push_back(reactivation.entry.resource_id);
            std::sort(action_ids.begin(), action_ids.end(), [](const auto& a, const auto& b) { return a.value < b.value; });
            if (action_ids.empty() || std::adjacent_find(action_ids.begin(), action_ids.end()) != action_ids.end()) { status={StatefulIssue::kInvalidApplicability}; return; }
            std::vector<ResourceExpectation> expectations = payload.expectations;
            std::sort(expectations.begin(), expectations.end(), [](const auto& a, const auto& b) { return a.resource_id.value < b.resource_id.value; });
            if (expectations.size() != action_ids.size()) { status={StatefulIssue::kInvalidApplicability}; return; }
            for (std::size_t i=0; i<action_ids.size(); ++i) if (expectations[i].resource_id != action_ids[i]) { status={StatefulIssue::kInvalidApplicability}; return; }
            ResourceManifest next = *context.manifest;
            for (const auto& entry : payload.upserts) {
                auto it = std::find_if(next.entries.begin(), next.entries.end(), [&](const auto& x){return x.resource_id==entry.resource_id;});
                const bool existed = it != next.entries.end();
                const auto previous = existed ? std::optional<ResourceManifestEntry>(*it) : std::nullopt;
                if (existed) { if (it->resource_kind != entry.resource_kind || entry.resource_revision != it->resource_revision+1) { status={StatefulIssue::kInvalidApplicability}; return; } *it=entry; }
                else { if (entry.resource_revision != 1 || std::find_if(next.retired_entries.begin(),next.retired_entries.end(),[&](const auto& x){return x.resource_id==entry.resource_id;})!=next.retired_entries.end()) { status={StatefulIssue::kInvalidApplicability}; return; } next.entries.push_back(entry); }
                ResourceManifestSemanticChange change{}; change.resource_id=entry.resource_id; change.flags=existed ? ResourceManifestChangeFlags::kUpdated : ResourceManifestChangeFlags::kCreated; if (previous) change.before.entry = *previous; change.after.entry=entry; plan.resource_changes.push_back(std::move(change));
            }
            for (const auto& id : payload.removes) { auto it=std::find_if(next.entries.begin(),next.entries.end(),[&](const auto& x){return x.resource_id==id;}); if(it==next.entries.end()){status={StatefulIssue::kObjectMissing};return;} for (const auto& object : context.objects.allObjects()) if (referencesResource(object, id)) { status={StatefulIssue::kInvalidReference}; return; } ResourceManifestTombstone t; t.resource_id=id; t.retired_revision=it->resource_revision+1; t.last_content_hash=it->content_hash; t.preimage=*it; t.removal_operation_id=operation.id; t.lineage_token=resourceManifestLineageToken(operation.document_id.value(), t); next.entries.erase(it); next.retired_entries.erase(std::remove_if(next.retired_entries.begin(), next.retired_entries.end(), [&](const auto& old){return old.resource_id == id;}), next.retired_entries.end()); next.retired_entries.push_back(t); ResourceManifestSemanticChange change{}; change.resource_id=id; change.flags=ResourceManifestChangeFlags::kRemoved; change.before.entry=t.preimage; change.after.tombstone=t; plan.resource_changes.push_back(std::move(change)); }
            for (const auto& reactivation : payload.reactivations) {
                auto tombstone = std::find_if(next.retired_entries.begin(), next.retired_entries.end(), [&](const auto& value) { return value.resource_id == reactivation.entry.resource_id; });
                if (tombstone == next.retired_entries.end() || tombstone->removal_operation_id.value() != reactivation.expected_removal_operation_id.value() || tombstone->preimage != reactivation.expected_preimage || tombstone->lineage_token != reactivation.expected_lineage_token || reactivation.entry.resource_revision != tombstone->retired_revision + 1U) { status={StatefulIssue::kInvalidApplicability}; return; }
                next.entries.push_back(reactivation.entry);
                ResourceManifestSemanticChange change{}; change.resource_id=reactivation.entry.resource_id; change.flags=ResourceManifestChangeFlags::kCreated; change.after.entry=reactivation.entry; plan.resource_changes.push_back(std::move(change));
            }
            for (const auto& expectation : expectations) {
                const auto active = std::find_if(context.manifest->entries.begin(), context.manifest->entries.end(), [&](const auto& value) { return value.resource_id == expectation.resource_id; });
                const auto retired = std::find_if(context.manifest->retired_entries.begin(), context.manifest->retired_entries.end(), [&](const auto& value) { return value.resource_id == expectation.resource_id; });
                const auto state = active != context.manifest->entries.end() ? ResourceExpectationState::kActive : retired != context.manifest->retired_entries.end() ? ResourceExpectationState::kRetired : ResourceExpectationState::kNeverUsed;
                const auto revision = state == ResourceExpectationState::kActive ? active->resource_revision : state == ResourceExpectationState::kRetired ? retired->retired_revision : 0U;
                if (expectation.state != state || expectation.revision != revision) { status={StatefulIssue::kInvalidApplicability}; return; }
            }
            if(payload.upserts.empty() && payload.removes.empty() && payload.reactivations.empty()){status={StatefulIssue::kInvalidApplicability};return;} if (next.manifest_revision == std::numeric_limits<ResourceRevision>::max()) { status={StatefulIssue::kInvalidApplicability}; return; } next.manifest_revision++; if(!normalizeResourceManifest(next)){status={StatefulIssue::kInvalidApplicability};return;} plan.manifest=std::move(next);
        }
    }, operation.payload);
    if (!status.ok()) return rejected(status);
    const auto validateTextReferences=[&](const ObjectRecord& object) {
        if(object.kind!=ObjectKind::kRichText || object.kind_version!=2U) return true;
        if(!context.manifest || !validateResourceManifest(*context.manifest)) return false;
        const auto* content=std::get_if<RichTextContent>(&object.content);if(!content)return false;
        for(const auto& paragraph:content->document.paragraphs) for(const auto& run:paragraph.runs) {
            if(!run.style.font_resource_id)return false;
            const auto found=std::find_if(context.manifest->entries.begin(),context.manifest->entries.end(),[&](const auto& entry){return entry.resource_id==*run.style.font_resource_id && entry.resource_kind==ResourceKind::kFont;});
            if(found==context.manifest->entries.end())return false;
        }
        return true;
    };
    for(const auto& object:plan.creates)if(!validateTextReferences(object))return rejected({StatefulIssue::kInvalidReference});
    for(const auto& object:plan.replacements)if(!validateTextReferences(object))return rejected({StatefulIssue::kInvalidReference});
    return {PrepareDisposition::kPrepared, {}, std::move(plan)};
}
} // namespace canvas::semantic
