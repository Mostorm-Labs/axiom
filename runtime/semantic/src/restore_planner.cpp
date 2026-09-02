#include "canvas/semantic/restore_planner.hpp"

#include "canvas/semantic/connector_validation.hpp"
#include "canvas/semantic/hierarchy_capability_validation.hpp"
#include "canvas/semantic/hierarchy_validation.hpp"
#include "canvas/semantic/operation_state_validator.hpp"
#include "canvas/semantic/staged_object_view.hpp"

#include <utility>
#include <vector>

namespace canvas::semantic {

StatefulResult validateRestoreObjects(
    const RestoreObjectsOp& restore,
    const ObjectStore& apply_base,
    RestorePlanInputs* out) {
    RestorePlanInputs result;
    StagedObjectView staged(apply_base);

    for (const ObjectRecord& candidate : restore.objects) {
        if (!staged.stageCreate(candidate)) {
            return StatefulResult{StatefulIssue::kObjectAlreadyExists};
        }
    }

    std::vector<HierarchyEdit> hierarchy_edits;
    hierarchy_edits.reserve(restore.objects.size());
    for (const ObjectRecord& candidate : restore.objects) {
        const StatefulResult record_result =
            validateRecordStateForOperation(candidate, StateRule::kCreateAbsent);
        if (!record_result.ok()) return record_result;
        hierarchy_edits.push_back(HierarchyEdit{candidate.id, candidate.placement});
    }

    const StatefulResult hierarchy_result = validateStagedHierarchy(staged, hierarchy_edits);
    if (!hierarchy_result.ok()) return hierarchy_result;

    std::vector<ObjectId> affected_child_ids;
    affected_child_ids.reserve(restore.objects.size());
    for (const ObjectRecord& candidate : restore.objects) {
        affected_child_ids.push_back(candidate.id);
    }
    const StatefulResult capability_result = validateStagedHierarchyCapabilities(
        staged, std::span<const ObjectId>(affected_child_ids.data(), affected_child_ids.size()));
    if (!capability_result.ok()) return capability_result;

    for (const ObjectRecord& candidate : restore.objects) {
        if (candidate.kind != ObjectKind::kConnector) continue;
        const auto* content = std::get_if<ConnectorContent>(&candidate.content);
        if (content == nullptr) return StatefulResult{StatefulIssue::kInvalidApplicability};
        const StatefulResult connector_result = validateConnectorReferences(staged, *content);
        if (!connector_result.ok()) return connector_result;
    }

    result.creates = restore.objects;
    *out = std::move(result);
    return StatefulResult{};
}

} // namespace canvas::semantic
