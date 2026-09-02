#include "canvas/semantic/apply_plan.hpp"

#include "canvas/semantic/idempotency.hpp"
#include "canvas/semantic/staged_object_view.hpp"

#include <utility>
#include <variant>

namespace canvas::semantic {
namespace {
PrepareResult rejected(StatefulResult e) { return PrepareResult{PrepareDisposition::kRejected, e, std::nullopt}; }
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
        } else if constexpr (std::is_same_v<T, EditRichTextOp>) { ReplaceObjectsStatePlanInputs out; status=validateEditRichTextState(payload,context.objects,&out); if(status.ok()) plan.replacements=std::move(out.replacements);
        } else if constexpr (std::is_same_v<T, SetConnectorContentOp>) { ReplaceObjectsStatePlanInputs out; status=validateSetConnectorContentState(payload,context.objects,&out); if(status.ok()) plan.replacements=std::move(out.replacements); }
    }, operation.payload);
    if (!status.ok()) return rejected(status);
    return {PrepareDisposition::kPrepared, {}, std::move(plan)};
}
} // namespace canvas::semantic
